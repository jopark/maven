#include "peakdetectiondialog.h"

PeakDetectionDialog::PeakDetectionDialog(QWidget *parent) : 
    QDialog(parent) {
    setupUi(this);
    settings = NULL;
    mainwindow = NULL;

    peakupdater = NULL;
    connect(computeButton, SIGNAL(clicked(bool)), SLOT(findPeaks()));
    connect(cancelButton, SIGNAL(clicked(bool)), SLOT(cancel()));
    connect(loadModelButton, SIGNAL(clicked(bool)), SLOT(loadModel()));
    connect(setOutputDirButton, SIGNAL(clicked(bool)), SLOT(setOutputDir()));

    setModal(true);

    _featureDetectionType= CompoundDB;

}

PeakDetectionDialog::~PeakDetectionDialog() { 
	cancel();
	if (peakupdater) delete(peakupdater);
}

void PeakDetectionDialog::cancel() { 

	if (peakupdater) {
		if( peakupdater->isRunning() ) { peakupdater->stop();  return; }
	}
	close(); 
}

void PeakDetectionDialog::setFeatureDetection(FeatureDetectionType type) {
    _featureDetectionType = type;

    if (_featureDetectionType == QQQ ) {

            setWindowTitle("QQQ Peak Detection");
            grpMassSlicingMethod->show();
            lblMassSlicingMethod->setText("Mass slices are determined based on loaded data files (samples).");

            featureOptions->hide();
            dbOptions->hide();

            grpMatchingOptions->hide();

    } else if (_featureDetectionType == FullSpectrum ) {

            setWindowTitle("Peak Detection");
            grpMassSlicingMethod->show();
            lblMassSlicingMethod->setText("Mass slices are determined based on loaded data files (samples).");

            featureOptions->show();
            dbOptions->hide();

            grpMatchingOptions->show();

            QLayout *oldLayout = tabPeakDetection->layout();
            delete(oldLayout);

            QGridLayout *gridLayout_10 = new QGridLayout(tabPeakDetection);
            gridLayout_10->setObjectName(QString::fromUtf8("gridLayout_10"));
            gridLayout_10->setContentsMargins(-1, -1, -1, 16);

            gridLayout_10->addWidget(grpMassSlicingMethod, 0, 1, 1, 1);
            gridLayout_10->addWidget(featureOptions, 1, 1, 2, 1);
            gridLayout_10->addWidget(eicOptions, 3, 1, 2, 1);

            tabPeakDetection->setLayout(gridLayout_10);


    } else if (_featureDetectionType == CompoundDB ) {

            setWindowTitle("Compound DB Search");
            grpMassSlicingMethod->show();
            lblMassSlicingMethod->setText("Mass slices are derived from compound theoretical m/z.");

            featureOptions->hide();
            dbOptions->show();

            grpMatchingOptions->show();

            QLayout *oldLayout = tabPeakDetection->layout();
            delete(oldLayout);

            QGridLayout *gridLayout_10 = new QGridLayout(tabPeakDetection);
            gridLayout_10->setObjectName(QString::fromUtf8("gridLayout_10"));
            gridLayout_10->setContentsMargins(-1, -1, -1, 16);

            gridLayout_10->addWidget(grpMassSlicingMethod, 0, 1, 1, 1);
            gridLayout_10->addWidget(dbOptions, 1, 1, 2, 1);
            gridLayout_10->addWidget(eicOptions, 3, 1, 2, 1);

            tabPeakDetection->setLayout(gridLayout_10);

    }

	adjustSize();
}
void PeakDetectionDialog::loadModel() { 

    const QString name = QFileDialog::getOpenFileName(this,
				"Select Classification Model", ".",tr("Model File (*.model)"));

	classificationModelFilename->setText(name); 
	Classifier* clsf = mainwindow->getClassifier();	//get classification model
	if (clsf ) clsf->loadModel( classificationModelFilename->text().toStdString() );
}

void PeakDetectionDialog::setOutputDir() { 
    const QString dirName = QFileDialog::getExistingDirectory(this, 
					"Save Reports to a Folder", ".", QFileDialog::ShowDirsOnly);
	outputDirName->setText(dirName);
}

void PeakDetectionDialog::show() {

    if ( mainwindow != NULL ) {
        QSettings* settings = mainwindow->getSettings();
        if ( settings ) {
            eic_smoothingWindow->setValue(settings->value("eic_smoothingWindow").toDouble());
            grouping_maxRtDiff->setValue(settings->value("grouping_maxRtWindow").toDouble());
        }
    }

    QString text = compoundDatabase->currentText();

    QStringList dbnames = DB.getLoadedDatabaseNames();
    dbnames.push_front("ALL");
    compoundDatabase->clear();
    for(QString db: dbnames ) {
        compoundDatabase->addItem(db);
    }

    //Issue 166: if a previous text entry was used, take this value.
    if (!text.isEmpty()) {
         int index = compoundDatabase->findText(text);
         if (index != -1) {
             compoundDatabase->setCurrentIndex(index);
         }
    } else {
        QString selectedDB = mainwindow->ligandWidget->getDatabaseName();
        compoundDatabase->setCurrentIndex(compoundDatabase->findText(selectedDB));
    }

    fragScoringAlgorithm->clear();
    for(string scoringAlgorithm: FragmentationMatchScore::getScoringAlgorithmNames()) {
        fragScoringAlgorithm->addItem(scoringAlgorithm.c_str());
    }

    compoundPPMWindow->setValue( mainwindow->getUserPPM() );  //total ppm window, not half sized.

    QDialog::show();
}

void PeakDetectionDialog::findPeaks() {
        if (mainwindow == nullptr) return;

		vector<mzSample*>samples = mainwindow->getSamples();
		if ( samples.size() == 0 ) return;

        if (peakupdater)  {
			if ( peakupdater->isRunning() ) cancel();
			if ( peakupdater->isRunning() ) return;
		}

        if (peakupdater) {
            delete(peakupdater);
            peakupdater=nullptr;
		}

		peakupdater = new BackgroundPeakUpdate(this);
		peakupdater->setMainWindow(mainwindow);

		connect(peakupdater, SIGNAL(updateProgressBar(QString,int,int)), SLOT(setProgressBar(QString, int,int)));
	
        if (settings) {
             peakupdater->eic_ppmWindow = settings->value("eic_ppmWindow").toDouble();
             peakupdater->eic_smoothingAlgorithm = settings->value("eic_smoothingAlgorithm").toInt();
		}

        peakupdater->baseline_smoothingWindow = baseline_smoothing->value();
        peakupdater->baseline_dropTopX        = baseline_quantile->value();
		peakupdater->eic_smoothingWindow= eic_smoothingWindow->value();
		peakupdater->grouping_maxRtWindow = grouping_maxRtDiff->value();
		peakupdater->matchRtFlag =  matchRt->isChecked();
        peakupdater->featureMatchRtFlag = this->featureMatchRts->isChecked();
		peakupdater->minGoodPeakCount = minGoodGroupCount->value();
		peakupdater->minNoNoiseObs = minNoNoiseObs->value();
		peakupdater->minSignalBaseLineRatio = sigBaselineRatio->value();
		peakupdater->minSignalBlankRatio = sigBlankRatio->value();
		peakupdater->minGroupIntensity = minGroupIntensity->value();
        peakupdater->pullIsotopesFlag = reportIsotopes->isChecked();
        peakupdater->ppmMerge =  ppmStep->value();
        peakupdater->compoundPPMWindow = compoundPPMWindow->value();
		peakupdater->compoundRTWindow = compoundRTWindow->value();

        float averageFullScanTime = 0;
        for (auto sample : samples){
            averageFullScanTime += sample->getAverageFullScanTime();
        }
        averageFullScanTime /= samples.size();

        peakupdater->avgScanTime = averageFullScanTime;

        peakupdater->rtStepSize = rtStep->value();
        peakupdater->mustHaveMS2 = featureMustHaveMs2->isChecked();
        peakupdater->compoundMustHaveMs2 = compoundMustHaveMS2->isChecked();
        peakupdater->productPpmTolr = productPpmTolr->value();
        peakupdater->scoringScheme  = fragScoringAlgorithm->currentText();
        peakupdater->minFragmentMatchScore = fragMinScore->value();
        peakupdater->minNumFragments = fragMinPeaks->value();
        peakupdater->minNumDiagnosticFragments = spnMinDiagnostic->value();
        peakupdater->excludeIsotopicPeaks = excludeIsotopicPeaks->isChecked();

        string policyText = peakGroupCompoundMatchPolicyBox->itemText(peakGroupCompoundMatchPolicyBox->currentIndex()).toStdString();

        if (policyText == "All matches"){
            peakupdater->peakGroupCompoundMatchingPolicy = ALL_MATCHES;
        } else if (policyText == "All matches with highest MS2 score") {
            peakupdater->peakGroupCompoundMatchingPolicy = TOP_SCORE_HITS;
        } else if (policyText == "One match with highest MS2 score (earliest alphabetically)") {
            peakupdater->peakGroupCompoundMatchingPolicy = SINGLE_TOP_HIT;
        }

        if ( ! outputDirName->text().isEmpty()) {
            peakupdater->setOutputDir(outputDirName->text());
		    peakupdater->writeCSVFlag=true;
        } else {
		    peakupdater->writeCSVFlag=false;
		}

        peakupdater->isRequireMatchingAdduct = chkRequireAdductMatch->isChecked();
        peakupdater->isRetainUnmatchedCompounds = chkRetainUnmatched->isChecked();
        peakupdater->isClusterPeakGroups = chkClusterPeakgroups->isChecked();

		QString title;
		if (_featureDetectionType == FullSpectrum )  title = "Detected Features";
                else if (_featureDetectionType == CompoundDB ) title = "DB Search " + compoundDatabase->currentText();
                else if (_featureDetectionType == QQQ ) title = "QQQ DB Search " + compoundDatabase->currentText();
     
        title = mainwindow->getUniquePeakTableTitle(title);

		TableDockWidget* peaksTable = mainwindow->addPeaksTable(title);
		peaksTable->setWindowTitle(title);
        connect(peakupdater, SIGNAL(newPeakGroup(PeakGroup*,bool, bool)), peaksTable, SLOT(addPeakGroup(PeakGroup*,bool, bool)));
        connect(peakupdater, SIGNAL(finished()), peaksTable, SLOT(showAllGroups()));
   		connect(peakupdater, SIGNAL(terminated()), peaksTable, SLOT(showAllGroups()));
   		connect(peakupdater, SIGNAL(finished()), this, SLOT(close()));
   		connect(peakupdater, SIGNAL(terminated()), this, SLOT(close()));
		
		
		//RUN THREAD
		if ( _featureDetectionType == QQQ ) {
			runBackgroupJob("findPeaksQQQ");
		} else if ( _featureDetectionType == FullSpectrum ) {
			runBackgroupJob("processMassSlices");
        }  else {
            peakupdater->compoundDatabase = compoundDatabase->currentText();
            if(peakupdater->compoundDatabase == "ALL") {
                peakupdater->setCompounds( DB.compoundsDB );
            } else {
                peakupdater->setCompounds( DB.getCopoundsSubset(compoundDatabase->currentText().toStdString()) );
            }
			runBackgroupJob("computePeaks");
		}
}

void PeakDetectionDialog::runBackgroupJob(QString funcName) { 
    if (peakupdater == nullptr ) return;

	if ( peakupdater->isRunning() ) { 
			cancel(); 
	}

	if ( ! peakupdater->isRunning() ) { 
		peakupdater->setRunFunction(funcName);			//set thread function

        qDebug() << "peakdetectiondialog::runBackgroundJob(QString funcName) Started.";

        peakupdater->start();	//start a background thread

        qDebug() << "peakdetectiondialog::runBackgroundJob(QString funcName) Completed.";
    }
}

void PeakDetectionDialog::showInfo(QString text){ statusText->setText(text); }

void PeakDetectionDialog::setProgressBar(QString text, int progress, int totalSteps){
	showInfo(text);
	progressBar->setRange(0,totalSteps);
    progressBar->setValue(progress);
}

