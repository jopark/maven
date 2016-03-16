#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QRegExp>
#include <QFile>
#include <QTextStream>
#include <QDebug>


#include "mzSample.h"
#include "mzUtils.h"

class Database {

    public:
        Database(){ _connected = false; };

        Database(QString filename) {
            connect(filename);
            reloadAll();
        }
        ~Database() { closeAll(); }

		QSqlDatabase& getLigandDB() { return ligandDB; }
        void reloadAll();  //loads all tables
        void closeAll();
        bool connect(QString filename);
    
        vector<Adduct*> loadAdducts(string filename);
        void loadMethodsFolder(QString methodsFolder);
        int  loadCompoundsFile(QString filename);
        vector<Compound*> loadCompoundCSVFile(QString fileName);
        vector<Compound*> loadNISTLibrary(QString fileName);
        void loadCompoundsSQL();
        void saveCompoundsSQL(vector<Compound*> &compoundSet);
        void deleteCompoundsSQL( QString dbName);

		multimap<string,Compound*> keywordSearch(string needle);
		vector<string>   getCompoundReactions(string compound_id);

		void addCompound(Compound*c);
        void loadReactions(string modelName);

        vector<Compound*> getCopoundsSubset(string database);
        vector<Compound*> getKnowns();


        map<string,int>	  getDatabaseNames();
        map<string,int>   getChromotographyMethods();

        Compound* findSpeciesById(string id,string db);
		Compound* findSpeciesByPrecursor(float precursorMz, float productMz,int polarity,double amuQ1, double amuQ3);
        set<Compound*> findSpeciesByMass(float mz, float ppm);
        vector<Compound*> findSpeciesByName(string name, string dbname);

        vector<MassCalculator::Match> findMathchingCompounds(float mz, float ppm, float charge);
        vector<MassCalculator::Match> findMathchingCompoundsSLOW(float mz, float ppm, float charge);

		void loadRetentionTimes(QString method);
		void saveRetentionTime(Compound* c, float rt, QString method);

		void saveValidation(Peak* p);

        vector<Adduct*> adductsDB;
        vector<Adduct*> fragmentsDB;

        vector<Compound*> compoundsDB;
        QMap<string,Compound*> compoundIdMap;

      private:
		QSqlDatabase ligandDB;
        bool _connected;
    
};


#endif

