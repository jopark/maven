#include "background_directinfusion_update.h"

#include "directinfusionprocessor.h"

BackgroundDirectInfusionUpdate::BackgroundDirectInfusionUpdate(QWidget*){ }

BackgroundDirectInfusionUpdate::~BackgroundDirectInfusionUpdate() {
    //TODO: need to delete any pointers?
}

void BackgroundDirectInfusionUpdate::run(void) {

    QTime timer;
    timer.start();

    qDebug() << "Direct infusion analysis started.";
    emit(updateProgressBar("Initializing...", 0, samples.size()));

    for (unsigned int i = 0; i < samples.size(); i++){

        mzSample* sample = samples.at(i);

         string msgStart = string("Processing sample ")
                 .append(to_string(i+1))
                 .append("/")
                 .append(to_string(samples.size()))
                 .append(": \"")
                 .append(sample->sampleName)
                 .append("\"");

         emit(updateProgressBar(msgStart.c_str(), (i+1), samples.size()));

         /**
          * ACTUAL WORK IS DONE HERE
          */

         //TODO: actually get adductlist from main window
         vector<Adduct*> adducts;
         adducts.push_back(MassCalculator::MinusHAdduct);

         DirectInfusionProcessor::processSingleSample(sample, compounds, adducts, false);
    }

    qDebug() << "Direct infusion analysis completed in " <<timer.elapsed() << " msec.";
    emit(closeDialog());
    quit();

    return;
}
