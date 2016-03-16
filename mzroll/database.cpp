#include "database.h"

bool Database::connect(QString filename) {
    ligandDB = QSqlDatabase::addDatabase("QSQLITE", "ligandDB");
    ligandDB.setDatabaseName(filename);
    ligandDB.open();

    if (!ligandDB.isOpen()) {
        qDebug()  << "Failed to open ligand database" + filename;
        return false;
    } else {
        return true;
    }
}

void Database::reloadAll() {

	//compounds subsets
    const std::string EmptyString;
    compoundIdMap.clear();
    compoundsDB.clear();
    loadCompoundsSQL();

    cerr << "compoundsDB=" << compoundsDB.size() << " " << compoundIdMap.size() << endl;
    cerr << "adductsDB=" << adductsDB.size() << endl;
    cerr << "fragmentsDB=" << fragmentsDB.size() << endl;
    cerr << endl;
}

void Database::loadMethodsFolder(QString methodsFolder) {
    QDir dir(methodsFolder);
    if (dir.exists()) {
        dir.setFilter(QDir::Files );
        QFileInfoList list = dir.entryInfoList();
        for (int i = 0; i < list.size(); ++i) {
            QFileInfo fileInfo = list.at(i);
            loadCompoundsFile(fileInfo.absoluteFilePath());
        }
    }

    loadCompoundsSQL();
}

int Database::loadCompoundsFile(QString filename) {
    string dbname = mzUtils::cleanFilename(filename.toStdString());

    vector<Compound*>compounds;

    if ( filename.endsWith("msp",Qt::CaseInsensitive)
         || filename.endsWith("sptxt",Qt::CaseInsensitive)) {
        compounds = loadNISTLibrary(filename);
    } else {
        compounds = loadCompoundCSVFile(filename);
    }

    deleteCompoundsSQL(dbname.c_str());
    saveCompoundsSQL(compounds);

    sort(compoundsDB.begin(),compoundsDB.end(), Compound::compMass);
    return compounds.size();
}


void Database::closeAll() {
    mzUtils::delete_all(adductsDB);     adductsDB.clear();
    mzUtils::delete_all(compoundsDB);   compoundsDB.clear();
    mzUtils::delete_all(fragmentsDB);   fragmentsDB.clear();
}

multimap<string,Compound*> Database::keywordSearch(string needle) { 
    QSqlQuery query(ligandDB);
    query.prepare("SELECT compound_id, keyword from sets where keyword like '%?%'");
	query.addBindValue(needle.c_str());
 	if (!query.exec())   qDebug() << query.lastError();
	

    multimap<string,Compound*>matches;
    while (query.next()) {
        std::string id  = query.value(0).toString().toStdString();
        std::string keyword  = query.value(1).toString().toStdString();
        Compound* cmpd = findSpeciesById(id,"");
        if (cmpd != NULL ) matches.insert(pair<string,Compound*>(keyword,cmpd));
    }
	return matches;
}

void Database::addCompound(Compound* c) { 
    if(c == NULL) return;
    compoundIdMap[c->id + c->db]=c;
    compoundsDB.push_back(c);
}

void Database::loadCompoundsSQL() {
        compoundsDB.clear();
        compoundIdMap.clear();

        QSqlQuery query(ligandDB);
        QString sql = "select * from compounds where name not like '%DECOY%'";
        query.prepare(sql);
        if(!query.exec()) qDebug() << query.lastError();

        while (query.next()) {
            string id =   query.value("compoundId").toString().toStdString();
            string name = query.value("name").toString().toStdString();
            string formula = query.value("formula").toString().toStdString();
            int charge = query.value("charge").toInt();

			Compound* compound = new Compound(id,name,formula,charge);
            compound->cid  =  query.value("cid").toInt();
            compound->db   =  query.value("dbName").toString().toStdString();
            compound->expectedRt =  query.value("expectedRt").toDouble();
            compound->precursorMz =  query.value("precursorMz").toDouble();
            compound->productMz =  query.value("productMz").toDouble();
            compound->collisionEnergy =  query.value("collisionEnergy").toDouble();
            compound->smileString=  query.value("smileString").toDouble();

            //mark compound as decoy if names contains DECOY string
            if(compound->name.find("DECOY") > 0) compound->isDecoy;

            for(QString f: query.value("category").toString().split(";") ) {
                compound->category.push_back(f.toStdString());
            }

            for(QString f: query.value("fragment_mzs").toString().split(";") ) {
                compound->fragment_mzs.push_back(f.toDouble());
            }

            for(QString f: query.value("fragment_intensity").toString().split(";") ) {
                compound->fragment_intensity.push_back(f.toDouble());
            }

            compoundIdMap[compound->id + compound->db]=compound;
            compoundsDB.push_back(compound);
        }
        sort(compoundsDB.begin(),compoundsDB.end(), Compound::compMass);
        qDebug() << "loadCompoundSQL : " << compoundsDB.size();
}

set<Compound*> Database::findSpeciesByMass(float mz, float ppm) { 
	set<Compound*>uniqset;

    Compound x("find", "", "",0);
    x.mass = mz-(mz/1e6*ppm);;
    vector<Compound*>::iterator itr = lower_bound(
            compoundsDB.begin(),compoundsDB.end(), 
            &x, Compound::compMass );

    for(;itr != compoundsDB.end(); itr++ ) {
        Compound* c = *itr;
        if (c->mass > mz+1) break;

        if ( mzUtils::ppmDist(c->mass,mz) < ppm ) {
			if (uniqset.count(c)) continue;
            uniqset.insert(c);
        }
    }
    return uniqset;
}

Compound* Database::findSpeciesById(string id,string db) {
    //cerr << "searching for " << id << " " << compoundIdMap.size() << " " << db << endl;
    if ( compoundIdMap.contains(id + db) ) return compoundIdMap[id + db];
    if ( compoundIdMap.contains(id) ) return compoundIdMap[id];

    return NULL;

    /*Compound* c = NULL;
    for(int i=0; i < compoundsDB.size(); i++ ) {
        if (compoundsDB[i]->id == id ) { c = compoundsDB[i]; break; }
    }
    return c;
    */
}


vector<Compound*> Database::findSpeciesByName(string name, string dbname) {
		vector<Compound*> set;
        qDebug() << "findSpeciesByName" << name.c_str();
		for(unsigned int i=0; i < compoundsDB.size(); i++ ) {
				if (compoundsDB[i]->name == name && compoundsDB[i]->db == dbname) {
					set.push_back(compoundsDB[i]);
				}
		}
		return set;
}

Compound* Database::findSpeciesByPrecursor(float precursorMz, float productMz, int polarity,double amuQ1, double amuQ3) {
		Compound* x=NULL;
		float dist=FLT_MAX;

		for(unsigned int i=0; i < compoundsDB.size(); i++ ) {
				if (compoundsDB[i]->precursorMz == 0 ) continue;
				//cerr << polarity << " " << compoundsDB[i]->charge << endl;
				if ((int) compoundsDB[i]->charge != polarity ) continue;
				float a = abs(compoundsDB[i]->precursorMz - precursorMz);
				if ( a > amuQ1 ) continue; // q1 tollorance
				float b = abs(compoundsDB[i]->productMz - productMz);
				if ( b > amuQ3 ) continue; // q2 tollarance
				float d = sqrt(a*a+b*b);
				if ( d < dist) { x = compoundsDB[i]; dist=d; }
		}
		return x;
}

vector<MassCalculator::Match> Database::findMathchingCompounds(float mz, float ppm, float charge) {
    vector<MassCalculator::Match>uniqset;

    //compute plausable adducts
    for(Adduct* a: adductsDB ) {
            if (SIGN(a->charge) != SIGN(charge)) continue;
            float pmz = a->computeParentMass(mz);
            set<Compound*>hits = this->findSpeciesByMass(pmz,ppm);

            for(Compound* c: hits) {
            float adductMass=a->computeAdductMass(c->mass);
            MassCalculator::Match match;
            match.name = c->name + " : " + a->name;
            match.adductLink = a;
            match.compoundLink = c;
            match.mass=adductMass;
            match.diff = mzUtils::ppmDist((double) adductMass, (double) mz);
            uniqset.push_back(match);
            //cerr << "F:" << match.name << " " << match.diff << endl;
        }
     }
    return uniqset;
}

vector<MassCalculator::Match> Database::findMathchingCompoundsSLOW(float mz, float ppm, float charge) {
    vector<MassCalculator::Match>uniqset;
    MassCalculator mcalc;

    for(Compound* c: compoundsDB) {
        float parentMass = mcalc.computeNeutralMass(c->formula);
        for(Adduct* a: adductsDB ) {
            if (SIGN(a->charge) != SIGN(charge)) continue;
                float adductMass=a->computeAdductMass(parentMass);
                if (mzUtils::ppmDist((double) adductMass, (double) mz) < ppm ) {
                    MassCalculator::Match match;
                    match.name = c->name + " : " + a->name;
                    match.adductLink = a;
                    match.compoundLink = c;
                    match.mass=adductMass;
                    match.diff = mzUtils::ppmDist((double) adductMass, (double) mz);
                    uniqset.push_back(match);
                    //cerr << "S:" << match.name << " " << match.diff << endl;
                }
        }
     }
    return uniqset;
}

void Database::saveRetentionTime(Compound* c, float rt, QString method) {
	if (!c) return;

	cerr << "setExpectedRetentionTime() " << rt << endl;
	//QSqlDatabase db = QSqlDatabase::database("ligand.db");
	 ligandDB.transaction();
     QSqlQuery query(ligandDB);

	query.prepare("insert into knowns_times values (?,?,?)");
	query.addBindValue(QString(c->id.c_str()));
	query.addBindValue(method);
	query.addBindValue(QString::number(rt,'f',4) );

	if(!query.exec()) qDebug() << query.lastError();

	ligandDB.commit();
	query.clear();
}

vector<Compound*> Database::getCopoundsSubset(string dbname) {
	vector<Compound*> subset;
	for (unsigned int i=0; i < compoundsDB.size(); i++ ) {
			if (compoundsDB[i]->db == dbname) { 
					subset.push_back(compoundsDB[i]);
			}
	}
	return subset;
}

map<string,int> Database::getDatabaseNames() { 
	map<string,int>dbnames;
	for (unsigned int i=0; i < compoundsDB.size(); i++ ) dbnames[ compoundsDB[i]->db ]++;
	return dbnames;
}

vector<Adduct*> Database::loadAdducts(string filename) {
    vector<Adduct*> adducts;
    ifstream myfile(filename.c_str());

    if (! myfile.is_open()) adducts;

    string line;
    while ( getline(myfile,line) ) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;

        vector<string>fields;
        mzUtils::split(line,',', fields);

        //ionization
        if(fields.size() < 2 ) continue;
        string name=fields[0];
        int nmol=string2float(fields[1]);
        int charge=string2float(fields[2]);
        float mass=string2float(fields[3]);

        if ( name.empty() || nmol < 0 ) continue;
        Adduct* a = new Adduct();
        a->nmol = nmol;
        a->name = name;
        a->mass = mass;
        a->charge = charge;
        a->isParent = false;
        if (abs(abs(a->mass)-HMASS)< 0.01) a->isParent=true;
        adducts.push_back(a);
    }
    cerr << "loadAdducts() " << filename << " count=" << adducts.size() << endl;
    return adducts;
    myfile.close();
}



vector<Compound*> Database::loadCompoundCSVFile(QString fileName){

    vector<Compound*> compoundSet; //return

    ifstream myfile(fileName.toLatin1());
    if (! myfile.is_open()) return compoundSet;

    string line;
    string dbname = mzUtils::cleanFilename(fileName.toStdString());
    int loadCount=0; 
    int lineCount=0;
    map<string, int>header;
    vector<string> headers;
    MassCalculator mcalc;

    //assume that files are tab delimited, unless matched ".csv", then comma delimited
    char sep='\t';
    if(fileName.endsWith(".csv",Qt::CaseInsensitive)) sep = ',';

    while ( getline(myfile,line) ) {
        if (!line.empty() && line[0] == '#') continue;
        //trim spaces on the left
        line.erase(line.find_last_not_of(" \n\r\t")+1);
        lineCount++;
        vector<string>fields;
        mzUtils::split(line, sep, fields);

        for(unsigned int i=0; i < fields.size(); i++ ) {
            int n = fields[i].length();
            if (n>2 && fields[i][0] == '"' && fields[i][n-1] == '"') {
                fields[i]= fields[i].substr(1,n-2);
            }
            if (n>2 && fields[i][0] == '\'' && fields[i][n-1] == '\'') {
                fields[i]= fields[i].substr(1,n-2);
            }
        }

        if (lineCount==1) {
            headers = fields;
            for(unsigned int i=0; i < fields.size(); i++ ) {
                fields[i]=makeLowerCase(fields[i]);
                header[ fields[i] ] = i;
            }
            continue;
        }

        string id, name, formula,smile;
        float rt=0;
        float mz=0;
        float charge=0;
        float collisionenergy=0;
        float precursormz=0;
        float productmz=0;
        float logP;
        int N=fields.size();
        vector<string>categorylist;

        if ( header.count("mz") && header["mz"]<N)  mz = string2float(fields[ header["mz"]]);
        if ( header.count("rt") && header["rt"]<N)  rt = string2float(fields[ header["rt"]]);
        if ( header.count("expectedrt") && header["expectedrt"]<N) rt = string2float(fields[ header["expectedrt"]]);
        if ( header.count("charge")&& header["charge"]<N) charge = string2float(fields[ header["charge"]]);
        if ( header.count("formula")&& header["formala"]<N) formula = fields[ header["formula"] ];
        if ( header.count("id")&& header["id"]<N) 	 id = fields[ header["id"] ];
        if ( header.count("name")&& header["name"]<N) 	 name = fields[ header["name"] ];
        if ( header.count("compound")&& header["compound"]<N) 	 name = fields[ header["compound"] ];
        if ( header.count("smile")&& header["smile"]<N) 	 name = fields[ header["smile"] ];
        if ( header.count("logp")&& header["logp"]<N) 	 logP = string2float(fields[ header["logp"] ]);

        if ( header.count("precursormz") && header["precursormz"]<N) precursormz=string2float(fields[ header["precursormz"]]);
        if ( header.count("productmz") && header["productmz"]<N)  productmz = string2float(fields[header["productmz"]]);
        if ( header.count("collisionenergy") && header["collisionenergy"]<N) collisionenergy=string2float(fields[ header["collisionenergy"]]);

        if ( header.count("Q1") && header["Q1"]<N) precursormz=string2float(fields[ header["Q1"]]);
        if ( header.count("Q3") && header["Q3"]<N)  productmz = string2float(fields[header["Q3"]]);
        if ( header.count("CE") && header["CE"]<N) collisionenergy=string2float(fields[ header["CE"]]);

        //cerr << lineCount << " " << endl;
        //for(int i=0; i<headers.size(); i++) cerr << headers[i] << ", ";
        //cerr << "   -> category=" << header.count("category") << endl;
        if ( header.count("category") && header["category"]<N) {
            string catstring=fields[header["category"]];
            if (!catstring.empty()) {
                mzUtils::split(catstring,';', categorylist);
                if(categorylist.size() == 0) categorylist.push_back(catstring);
                //cerr << catstring << " ListSize=" << categorylist.size() << endl;
            }
         }

        if ( header.count("polarity") && header["polarity"] <N)  {
            string x = fields[ header["polarity"]];
            if ( x == "+" ) {
                charge = 1;
            } else if ( x == "-" ) {
                charge = -1;
            } else  {
                charge = string2float(x);
            }

        }

        //cerr << "Loading: " << id << " " << formula << "mz=" << mz << " rt=" << rt << " charge=" << charge << endl;

        if (mz == 0) mz=precursormz;
        if (id.empty()&& !name.empty()) id=name;
        if (id.empty() && name.empty()) id="cmpd:" + integer2string(loadCount);

        if ( mz > 0 || ! formula.empty() ) {
            Compound* compound = new Compound(id,name,formula,charge);

            compound->expectedRt = rt;
            if (mz == 0) mz = mcalc.computeMass(formula,charge);
            compound->mass = mz;
            compound->db = dbname;
            compound->expectedRt=rt;
            compound->precursorMz=precursormz;
            compound->productMz=productmz;
            compound->collisionEnergy=collisionenergy;
            compound->smileString=smile;
            compound->logP=logP;
            for(int i=0; i < categorylist.size(); i++) compound->category.push_back(categorylist[i]);
            compoundSet.push_back(compound);
            //addCompound(compound);
            loadCount++;
        }
    }
    myfile.close();

    sort(compoundSet.begin(),compoundSet.end(), Compound::compMass);
    return compoundSet;
}

vector<Compound*> Database::loadNISTLibrary(QString fileName) {
    qDebug() << "Loading NIST Libary: " << fileName;
    QFile data(fileName);

    vector<Compound*> compoundSet;
    if (!data.open(QFile::ReadOnly) ) {
        qDebug() << "Can't open " << fileName; exit(-1);
        return compoundSet;
    }

    string dbfilename = fileName.toStdString();
    string dbname = mzUtils::cleanFilename(dbfilename);

   QTextStream stream(&data);
   QRegExp whiteSpace("\\s+");
   QRegExp formulaMatch("(C\\d+H\\d+\\S*)");
   QRegExp retentionTimeMatch("AvgRt\\=(\\S+)");

   QString line;
   MassCalculator mcalc;
   Compound* cpd = NULL;
   bool capturePeaks=false;

    do {
        line = stream.readLine();
        if(line.startsWith("Name:",Qt::CaseInsensitive)){

            if(cpd and !cpd->name.empty()) {
                //cerr << "NIST LIBRARY:" << cpd->db << " " << cpd->name << " " << cpd->formula << " " << cpd->id << endl;
                if (cpd->precursorMz>0) { cpd->mass=cpd->precursorMz; }
                else if (cpd->mass>0)   { cpd->precursorMz=cpd->mass; }
                else if(!cpd->formula.empty()) { cpd->mass = cpd->precursorMz = mcalc.computeMass(cpd->formula,0); }
                compoundSet.push_back(cpd);
            }

            //NEW COMPOUND
            QString name = line.mid(5,line.length()).simplified();
            cpd = new Compound(name.toStdString(), name.toStdString(),"", 0);
            cpd->db = dbname;
            capturePeaks=false;
        }

        if(cpd == NULL) continue;

        if (line.startsWith("MW:",Qt::CaseInsensitive)) {
            cpd->mass = line.mid(3,line.length()).simplified().toDouble();
         } else if (line.startsWith("CE:",Qt::CaseInsensitive)) {
            cpd->collisionEnergy = line.mid(3,line.length()).simplified().toDouble();
         } else if (line.startsWith("ID:",Qt::CaseInsensitive)) {
            QString id = line.mid(3,line.length()).simplified();
            if(!id.isEmpty()) cpd->id = id.toStdString();
         } else if (line.startsWith("LOGP:",Qt::CaseInsensitive)) {
            cpd->logP = line.mid(5,line.length()).simplified().toDouble();
         } else if (line.startsWith("SMILE:",Qt::CaseInsensitive)) {
            QString smileString = line.mid(7,line.length()).simplified();
            if(!smileString.isEmpty()) cpd->smileString=smileString.toStdString();
         } else if (line.startsWith("PRECURSORMZ:",Qt::CaseInsensitive)) {
            cpd->precursorMz = line.mid(13,line.length()).simplified().toDouble();
         } else if (line.startsWith("FORMULA:",Qt::CaseInsensitive)) {
             QString formula = line.mid(9,line.length()).simplified();
             formula.replace("\"","",Qt::CaseInsensitive);
             if(!formula.isEmpty()) cpd->formula = formula.toStdString();
         } else if (line.startsWith("CATEGORY:",Qt::CaseInsensitive)) {
             cpd->category.push_back(line.mid(10,line.length()).simplified().toStdString());
         } else if (line.startsWith("Comment:",Qt::CaseInsensitive)) {
             QString comment = line.mid(8,line.length()).simplified();
             if (comment.contains(formulaMatch)){
                 cpd->formula=formulaMatch.capturedTexts().at(1).toStdString();
                 //qDebug() << "Formula=" << formula;
             }
            if (comment.contains(retentionTimeMatch)){
               cpd->expectedRt=retentionTimeMatch.capturedTexts().at(1).simplified().toDouble();
                 //qDebug() << "retentionTime=" << retentionTimeString;
             }
         } else if (line.startsWith("Num Peaks:",Qt::CaseInsensitive) || line.startsWith("NumPeaks:",Qt::CaseInsensitive)) {
             capturePeaks=true;
         } else if (capturePeaks ) {
             QStringList mzintpair = line.split(whiteSpace);
             if( mzintpair.size() >=2 ) {
                 bool ok=false; bool ook=false;
                 float mz = mzintpair.at(0).toDouble(&ok);
                 float ints = mzintpair.at(1).toDouble(&ook);
                 if (ok && ook && mz >= 0 && ints >= 0) {
                     cpd->fragment_intensity.push_back(ints);
                     cpd->fragment_mzs.push_back(mz);
                 }
             }
         }

    } while (!line.isNull());

    //if (cpd) compoundSet.push_back(cpd);

    //compoundSet.push_back(compound);
    qDebug() << "Database::loadNISTLibrary() in" << compoundSet.size()  << " compounds.";
    //sort(compoundSet.begin(),compoundSet.end(), Compound::compMass);
    return compoundSet;
}


void Database::deleteCompoundsSQL(QString dbName) {
    QSqlQuery query(ligandDB);
    query.prepare("delete from compounds where dbName = ?");
    query.bindValue( 0, dbName );
    query.exec();
    qDebug() << "deleteCompoundsSQL" << dbName <<  " " << query.numRowsAffected();
}

void Database::saveCompoundsSQL(vector<Compound*> &compoundSet) {

    QSqlQuery query0(ligandDB);
    query0.exec("begin transaction");
    if(!query0.exec("create table IF NOT EXISTS compounds(\
                    cid integer primary key AUTOINCREMENT,\
                    dbName varchar(255),\
                    compoundId varchar(255),\
                    name varchar(255),\
                    formula varchar(255),\
                    smileString  varchar(255),\
                    srmId  varchar(255),\
                    mass float,\
                    charge  int,\
                    expectedRt float, \
                    precursorMz float,\
                    productMz   float,\
                    collisionEnergy float,\
                    logP float,\
                    category varchar(255),\
                    fragment_mzs text, \
                    fragment_intensity text \
                    )"))  qDebug() << "Ho... " << query0.lastError();

        QSqlQuery query1(ligandDB);
        query1.prepare("insert into compounds values(NULL,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?)");

        for(Compound* c : compoundSet) {
            QStringList cat;
            QStringList fragMz;
            QStringList fragIntensity;

            for(string s : c->category) { cat << s.c_str(); }
            for(float f :  c->fragment_mzs) { fragMz << QString::number(f,'f',5); }
            for(float f :  c->fragment_intensity) { fragIntensity << QString::number(f,'f',5); }

            query1.bindValue( 0, QString(c->db.c_str()) );
            query1.bindValue( 1, QString(c->id.c_str()) );
            query1.bindValue( 2, QString(c->name.c_str()));
            query1.bindValue( 3, QString(c->formula.c_str()));
            query1.bindValue( 4, QString(c->smileString.c_str()));
            query1.bindValue( 5, QString(c->srmId.c_str()));

            query1.bindValue( 6, c->mass );
            query1.bindValue( 7, c->charge);
            query1.bindValue( 8, c->expectedRt);
            query1.bindValue( 9, c->precursorMz);
            query1.bindValue( 10, c->productMz);

            query1.bindValue( 11, c->collisionEnergy);
            query1.bindValue( 12, c->logP);
            query1.bindValue( 13, cat.join(";"));
            query1.bindValue( 14, fragMz.join(";"));
            query1.bindValue( 15, fragIntensity.join(";"));

            if(!query1.exec())  qDebug() << query1.lastError();
        }

    query0.exec("end transaction");
}
