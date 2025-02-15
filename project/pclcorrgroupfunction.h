#ifndef PCLCORRGROUPFUNCTION_H
#define PCLCORRGROUPFUNCTION_H

#include <QtConcurrentMap>
#include <QVector>
#include <QMutex>

#include <pcl/io/pcd_io.h>
#include <pcl/correspondence.h>
#include <pcl/features/normal_3d_omp.h>
#include <pcl/features/shot_omp.h>
#include <pcl/features/board.h>

#include "logger.h"
#include "defines.h"

#ifndef PCL_1_8
    #include <pcl/keypoints/uniform_sampling.h>
#else
    #include <pcl/filters/uniform_sampling.h>
#endif

#include <pcl/recognition/cg/hough_3d.h>
#include <pcl/recognition/cg/geometric_consistency.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/kdtree/impl/kdtree_flann.hpp>
#include <pcl/common/transforms.h>
#include <pcl/console/parse.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>

#include <QElapsedTimer>


typedef pcl::Normal NormalType;
typedef pcl::ReferenceFrame RFType;
typedef pcl::SHOT1344 DescriptorType; //352
typedef boost::signals2::signal<void ()>  signal_t;
typedef pcl::PointCloud<NormalType>::Ptr normalsPtr;
typedef pcl::PointCloud<DescriptorType>::Ptr descriptorsPtr;

class PCLCorrGroupFunction
{
public:
    PCLCorrGroupFunction();
    cloudPtrType getCorrespondence();


    void recognize ();
    void loadCloudsFromDefaultFile();
    void loadSceneFromFile(std::string sceneFilename);
    void loadModelFromFile(std::string modelFilename);
    void setUpOffSceneModel();

    cloudPtrType computeKeypointsForThisModel(cloudPtrType &model);

    int getNrModelFound();

    int getComputationTimems();

    bool            useHough;
    bool            applyTrasformationToModel;
    bool            useCloudResolution;
    bool            computeModelKeypoints;

    float           modelSampleSize;
    float           sceneSampleSize;
    float           descriptorsRadius;
    float           referenceFrameRadius;
    float           cgSize;
    float           cgThreshold;
    std::string     modelFileName;
    std::string     sceneFileName;
    cloudPtrType        model;
    cloudPtrType        scene;
    cloudPtrType        offSceneModel;
    cloudPtrType        offSceneModelKeypoints;
    cloudPtrType        modelKeypoints;
    cloudPtrType        sceneKeypoints;
    normalsPtr      modelNormals;
    normalsPtr      sceneNormals;
    descriptorsPtr  modelDescriptors;
    descriptorsPtr  sceneDescriptors;

    pcl::KdTreeFLANN<DescriptorType> matchSearch;
    pcl::CorrespondencesPtr modelSceneCorrs;
    std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > rototranslations;
    std::vector<pcl::Correspondences> clusteredCorrs;

private:

    double computeCloudResolution (const cloudType::ConstPtr &cloud);
    void transformCloud (cloudType::Ptr &cloud);
    void setUpResolutionInvariance();
    void computeModelNormals();
    void computeModelDescriptors();
    void computeSceneNormals();
    void computeSceneDescriptors();
    void downSampleScene();
    void downSampleModel();
    void downSampleCloud(cloudPtrType &cloud,  float sampleSize, cloudPtrType &keypoints);
    void computeDescriptorsForKeypoints(cloudPtrType &cloud,  cloudPtrType &keypoints, normalsPtr &normals, descriptorsPtr &descriptors);
    void findCorrespondences();
    void parallelFindCorrespondence(DescriptorType &aDescriptor);

    void recognizeUsingHough();
    void recognizeUsingGeometricConsistency();
    void printResults();
    void resetValues ();
    QElapsedTimer* timer;


    QMutex parallelCorrProcessingMutex;
    int parallelCorrProcessingIndex;
};


#endif // PCLCORRGROUPFUNCTION_H
