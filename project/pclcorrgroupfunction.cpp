#include "pclcorrgroupfunction.h"

PCLCorrGroupFunction::PCLCorrGroupFunction()
{
    //setupDefaultValues();

    timer = new QElapsedTimer();
}

void PCLCorrGroupFunction::recognize ()
    {
        timer->start();
        resetValues();
        if (!this->model)
        {
            Logger::logInfo("Error model cloud is null.");
            return;
        }
        if (!this->scene)
        {
            Logger::logInfo("Error scene cloud is null.");
            return;
        }
        // TODO to be removed
        //if (applyTrasformationToModel)
        //    transformCloud(model);

        if (useCloudResolution)
            setUpResolutionInvariance();

        // Compute Descriptors
        if(computeModelKeypoints) {
            computeModelNormals();
            downSampleModel();
            computeModelDescriptors();
            computeModelKeypoints=false;
        }


        computeSceneNormals();
        downSampleScene();
        computeSceneDescriptors();

        // For each scene keypoint descriptor,find nearest neighbor into the model
        // keypoints descriptor cloud and add it to the correspondences vector.
        findCorrespondences();

        //  Clustering
        if (useHough)
            recognizeUsingHough();
        else
            recognizeUsingGeometricConsistency();

        printResults();
}

double PCLCorrGroupFunction::computeCloudResolution (const cloudType::ConstPtr &cloud)
    {
        double res = 0.0;
        int n_points = 0;
        int nres;
        std::vector<int> indices (2);
        std::vector<float> sqr_distances (2);
        pcl::search::KdTree<PointType> tree;
        tree.setInputCloud (cloud);

        for (size_t i = 0; i < cloud->size (); ++i)
        {
            if (! pcl_isfinite ((*cloud)[i].x))
                continue;
            //Considering the second neighbor since the first is the point itself.
            nres = tree.nearestKSearch (i, 2, indices, sqr_distances);
            if (nres == 2)
            {
                res += sqrt (sqr_distances[1]);
                ++n_points;
            }
        }
        if (n_points != 0)
            res /= n_points;
        return res;
    }

void PCLCorrGroupFunction::transformCloud(cloudPtrType &cloud){

    /*  METHOD #2: Using a Affine3f
     This method is easier and less error prone
     */
    Eigen::Affine3f transform = Eigen::Affine3f::Identity();
    float theta = M_PI/4; // The angle of rotation in radians

    // Define a translation of 2.5 meters on the x axis.
    transform.translation() << 0.0, 0.0, 0.0;

    // The same rotation matrix as before; tetha radians arround Z axis
    transform.rotate (Eigen::AngleAxisf (theta, Eigen::Vector3f::UnitZ()));

    // Print the transformation
    printf ("\nApplying Transform using an Affine3f\n");
    std::cout << transform.matrix() << std::endl;

    pcl::transformPointCloud (*cloud, *cloud, transform);
}

void PCLCorrGroupFunction::loadCloudsFromDefaultFile(){
    this->loadSceneFromFile("~/Qrecog/milk.pcd");
    this->loadModelFromFile("~/Qrecog/milk_cartoon_all_small_clorox.pcd");
}


void PCLCorrGroupFunction::loadModelFromFile(std::string modelFilename){
    if ( pcl::io::loadPCDFile (modelFilename, *(this->model)) < 0)
        std::cout << "Error loading default model cloud." << std::endl;
}

void PCLCorrGroupFunction::loadSceneFromFile(std::string sceneFilename){

    if ( pcl::io::loadPCDFile (sceneFilename, *(this->scene)) < 0)
        std::cout << "Error loading scene model cloud." << std::endl;
}


void PCLCorrGroupFunction::setUpResolutionInvariance(){
    float resolution = static_cast<float> (computeCloudResolution (model));
    if (resolution != 0.0f)
    {
        modelSampleSize          *= resolution;
        sceneSampleSize          *= resolution;
        referenceFrameRadius     *= resolution;
        descriptorsRadius        *= resolution;
        cgSize                   *= resolution;
    }
    Logger::logInfo("Model resolution:       "  + std::to_string(resolution));
    Logger::logInfo("Model sampling size:    "  + std::to_string(modelSampleSize));
    Logger::logInfo("Scene sampling size:    "  + std::to_string(sceneSampleSize));
    Logger::logInfo("LRF support radius:     "  + std::to_string(referenceFrameRadius));
    Logger::logInfo("SHOT descriptor radius: "  + std::to_string(descriptorsRadius));
    Logger::logInfo("Clustering bin size:    "  + std::to_string(cgSize));
}

void PCLCorrGroupFunction::computeModelNormals(){
    timer->restart();
    pcl::NormalEstimationOMP<PointType, NormalType> normEst;
    normEst.setKSearch (10);

    normEst.setInputCloud (model);
    normEst.compute (*modelNormals);
    Logger::logInfo("computeModelNormals time: " + std::to_string((int)timer->elapsed()));
}

void PCLCorrGroupFunction::computeSceneNormals(){
    timer->restart();
    pcl::NormalEstimationOMP<PointType, NormalType> normEst;
    normEst.setKSearch (10); //TODO: Verificare se mettere a vista

    normEst.setInputCloud (scene);
    normEst.compute (*sceneNormals);
    Logger::logInfo("computeSceneNormals time: " + std::to_string((int)timer->elapsed()));
}

void PCLCorrGroupFunction::downSampleScene(){
    timer->restart();
    downSampleCloud(scene, sceneSampleSize, sceneKeypoints);
    Logger::logInfo("downSampleScene time: " + std::to_string((int)timer->elapsed()));
    Logger::logInfo("TotalScenePoints: "    + std::to_string(scene->size()));
    Logger::logInfo("SceneKeypoints: "      + std::to_string(sceneKeypoints->size()));
}

void PCLCorrGroupFunction::downSampleModel(){
    timer->restart();
    downSampleCloud(model, modelSampleSize, modelKeypoints);
    Logger::logInfo("downSampleModel time: " + std::to_string((int)timer->elapsed()));
    Logger::logInfo("TotalModelPoints: "    + std::to_string(model->size()));
    Logger::logInfo("ModelKeypoints: "    + std::to_string(modelKeypoints->size()));
}

void PCLCorrGroupFunction::downSampleCloud(cloudPtrType &cloud,  float sampleSize, cloudPtrType &keypoints){    
    pcl::UniformSampling<PointType> uniform_sampling;
    uniform_sampling.setInputCloud (cloud);
    uniform_sampling.setRadiusSearch (sampleSize);

#ifndef PCL_1_8
    pcl::PointCloud<int> sampledIndices;
    uniform_sampling.compute (sampledIndices);
    pcl::copyPointCloud (*cloud, sampledIndices.points, *keypoints);
#else
    uniform_sampling.filter(*keypoints);
#endif
}

void PCLCorrGroupFunction::computeModelDescriptors(){
    timer->restart();
    computeDescriptorsForKeypoints(model, modelKeypoints, modelNormals, modelDescriptors);
    Logger::logInfo("computeModelDescriptors time: " + std::to_string((int)timer->elapsed()));
}

void PCLCorrGroupFunction::computeSceneDescriptors(){
    timer->restart();
    computeDescriptorsForKeypoints(scene, sceneKeypoints, sceneNormals, sceneDescriptors);
    Logger::logInfo("computeSceneDescriptors time: " + std::to_string((int)timer->elapsed()));
}

void PCLCorrGroupFunction::computeDescriptorsForKeypoints(cloudPtrType &cloud,  cloudPtrType &keypoints, normalsPtr &normals, descriptorsPtr &descriptors){
    pcl::SHOTColorEstimationOMP<PointType, NormalType, DescriptorType> descriptorEst;
    descriptorEst.setRadiusSearch (descriptorsRadius);

    descriptorEst.setInputCloud (keypoints);
    descriptorEst.setInputNormals (normals);
    descriptorEst.setSearchSurface (cloud);
    descriptorEst.compute (*descriptors);
}

void PCLCorrGroupFunction::findCorrespondences(){
    timer->restart();
    parallelCorrProcessingIndex=0;
    matchSearch.setInputCloud (modelDescriptors);

    QVector<DescriptorType> sequence;
    for (size_t i = 0; i < sceneDescriptors->size (); ++i)
        sequence.push_back(sceneDescriptors->at(i));

    QFuture<void>future = QtConcurrent::map(sequence,
                      [&](DescriptorType i){parallelFindCorrespondence(i);});
    future.waitForFinished();

    /*
    for (size_t i = 0; i < sceneDescriptors->size (); ++i)
    {
        std::vector<int> neighIndices (1);
        std::vector<float> neighSqrDists (1);
        if (!pcl_isfinite (sceneDescriptors->at (i).descriptor[0])) //skipping NaNs
            continue;
        int foundNeighs = matchSearch.nearestKSearch (sceneDescriptors->at (i), 1, neighIndices, neighSqrDists);
        //  add match only if the squared descriptor distance is less than 0.25 (SHOT descriptor distances are between 0 and 1 by design)
        if(foundNeighs == 1 && neighSqrDists[0] < 0.25f)
        {
            pcl::Correspondence corr (neighIndices[0], static_cast<int> (i), neighSqrDists[0]);
            modelSceneCorrs->push_back (corr);
        }
    }
    */

    Logger::logInfo("findCorrespondences time: " + std::to_string((int)timer->elapsed()));
    Logger::logInfo("Correspondences found: " + std::to_string(modelSceneCorrs->size ()));
}

void PCLCorrGroupFunction::parallelFindCorrespondence(DescriptorType &aDescriptor)
{
    std::vector<int> neighIndices (1);
    std::vector<float> neighSqrDists (1);
    if (!pcl_isfinite (aDescriptor.descriptor[0])) //skipping NaNs
        return;
    int foundNeighs = matchSearch.nearestKSearch (aDescriptor, 1, neighIndices, neighSqrDists);
    //  add match only if the squared descriptor distance is less than 0.25 (SHOT descriptor distances are between 0 and 1 by design)
    if(foundNeighs == 1 && neighSqrDists[0] < 0.25f)
    {
        parallelCorrProcessingMutex.lock();
        pcl::Correspondence corr (neighIndices[0], static_cast<int> (parallelCorrProcessingIndex), neighSqrDists[0]);
        parallelCorrProcessingIndex++;
        modelSceneCorrs->push_back (corr);
        parallelCorrProcessingMutex.unlock();
    }
}

void PCLCorrGroupFunction::recognizeUsingHough(){
    timer->restart();
    //
    //  Compute (Keypoints) Reference Frames only for Hough
    //
    pcl::PointCloud<RFType>::Ptr modelRf (new pcl::PointCloud<RFType> ());
    pcl::PointCloud<RFType>::Ptr sceneRf (new pcl::PointCloud<RFType> ());

    pcl::BOARDLocalReferenceFrameEstimation<PointType, NormalType, RFType> rfEst;
    rfEst.setFindHoles (true);
    rfEst.setRadiusSearch (referenceFrameRadius);

    rfEst.setInputCloud (modelKeypoints);
    rfEst.setInputNormals (modelNormals);
    rfEst.setSearchSurface (model);
    rfEst.compute (*modelRf);

    rfEst.setInputCloud (sceneKeypoints);
    rfEst.setInputNormals (sceneNormals);
    rfEst.setSearchSurface (scene);
    rfEst.compute (*sceneRf);

    //  Clustering
    pcl::Hough3DGrouping<PointType, PointType, RFType, RFType> clusterer;
    clusterer.setHoughBinSize (cgSize);
    clusterer.setHoughThreshold (cgThreshold);
    clusterer.setUseInterpolation (true);
    clusterer.setUseDistanceWeight (false);

    clusterer.setInputCloud (modelKeypoints);
    clusterer.setInputRf (modelRf);
    clusterer.setSceneCloud (sceneKeypoints);
    clusterer.setSceneRf (sceneRf);
    clusterer.setModelSceneCorrespondences (modelSceneCorrs);

    //clusterer.cluster (clustered_corrs);
    clusterer.recognize (rototranslations, clusteredCorrs);
    Logger::logInfo("recognizeUsingHough time: " + std::to_string((int)timer->elapsed()));

}

void PCLCorrGroupFunction::recognizeUsingGeometricConsistency(){
    timer->restart();
    pcl::GeometricConsistencyGrouping<PointType, PointType> gcClusterer;
    gcClusterer.setGCSize (cgSize);
    gcClusterer.setGCThreshold (cgThreshold);

    gcClusterer.setInputCloud (modelKeypoints);
    gcClusterer.setSceneCloud (sceneKeypoints);
    gcClusterer.setModelSceneCorrespondences (modelSceneCorrs);

    //gc_clusterer.cluster (clusteredCorrs);
    gcClusterer.recognize (rototranslations, clusteredCorrs);
    Logger::logInfo("recognizeUsingGeometricConsistency time: " + std::to_string((int)timer->elapsed()));

}

void PCLCorrGroupFunction::printResults(){
    std::cout << "Model instances found: " << rototranslations.size () << std::endl;
    for (size_t i = 0; i < rototranslations.size (); ++i)
    {
        Logger::logInfo("\n    Instance " + std::to_string(i + 1) + ":");
        Logger::logInfo("        Correspondences belonging to this instance: " + std::to_string(clusteredCorrs[i].size ()));

        // Print the rotation matrix and translation vector
        Eigen::Matrix3f rotation = rototranslations[i].block<3,3>(0, 0);
        Eigen::Vector3f translation = rototranslations[i].block<3,1>(0, 3);

        Logger::logInfo("\n");
        Logger::logInfo("            | %6.3f %6.3f %6.3f | \n" + std::to_string(rotation (0,0)) + std::to_string(rotation (0,1)) + std::to_string(rotation (0,2)));
        Logger::logInfo("        R = | %6.3f %6.3f %6.3f | \n" + std::to_string(rotation (1,0)) + std::to_string(rotation (1,1)) + std::to_string(rotation (1,2)));
        Logger::logInfo("            | %6.3f %6.3f %6.3f | \n" + std::to_string(rotation (2,0)) + std::to_string(rotation (2,1)) + std::to_string(rotation (2,2)));
        Logger::logInfo("\n");
        Logger::logInfo("        t = < %0.3f, %0.3f, %0.3f >\n" + std::to_string(rotation (0)) + std::to_string(rotation (1)) + std::to_string(rotation (2)));
    }

}

void PCLCorrGroupFunction::resetValues (){

    sceneDescriptors   = (descriptorsPtr)new pcl::PointCloud<DescriptorType> ();
    sceneKeypoints     = (cloudPtrType)new cloudType ();
    sceneNormals       = (normalsPtr)new pcl::PointCloud<NormalType> ();

    if(computeModelKeypoints) {
        modelKeypoints     = (cloudPtrType)new cloudType();
        modelNormals       = (normalsPtr)new pcl::PointCloud<NormalType> ();
        modelDescriptors   = (descriptorsPtr)new pcl::PointCloud<DescriptorType> ();
        parallelCorrProcessingIndex=0;
    }

    modelSceneCorrs    = (pcl::CorrespondencesPtr)new pcl::Correspondences ();
    rototranslations.clear();
    clusteredCorrs.clear();
}

void PCLCorrGroupFunction::setUpOffSceneModel()
{
    offSceneModelKeypoints  = (cloudPtrType)new cloudType ();
    offSceneModel           = (cloudPtrType)new cloudType ();
    //  We are translating the model so that it doesn't end in the middle of the scene representation
    pcl::transformPointCloud (*model, *offSceneModel, Eigen::Vector3f (-1,0,0), Eigen::Quaternionf (1, 0, 0, 0));
    pcl::transformPointCloud (*modelKeypoints, *offSceneModelKeypoints, Eigen::Vector3f (-1,0,0), Eigen::Quaternionf (1, 0, 0, 0));
}

cloudPtrType PCLCorrGroupFunction::computeKeypointsForThisModel(cloudPtrType &model)
{
    this->model = model;
    modelKeypoints     = (cloudPtrType)new cloudType();
    modelNormals       = (normalsPtr)new pcl::PointCloud<NormalType> ();
    modelDescriptors   = (descriptorsPtr)new pcl::PointCloud<DescriptorType> ();
    computeModelNormals();
    downSampleModel();
    computeDescriptorsForKeypoints(model, modelKeypoints, modelNormals, modelDescriptors);
    return modelKeypoints;
}

int PCLCorrGroupFunction::getNrModelFound()
{
    return rototranslations.size();
}

int PCLCorrGroupFunction::getComputationTimems()
{
    return timer->elapsed();
}

cloudPtrType PCLCorrGroupFunction::getCorrespondence()
{
    cloudPtrType rotated_model (new cloudType ());
    for (size_t i = 0; i < rototranslations.size (); ++i)
        pcl::transformPointCloud (*model, *rotated_model, rototranslations[i]);

    for (size_t i = 0; i< rotated_model->points.size(); i++) {
        rotated_model->points[i].r = 255;
        rotated_model->points[i].b = 0;
        rotated_model->points[i].g = 0;
    }
    return rotated_model;
}
