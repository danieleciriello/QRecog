#include "pclclusteringfunction.h"

PCLClusteringFunction::PCLClusteringFunction()
    : tree (new pcl::search::KdTree<PointType>),
      workingCloud(new cloudType)
{
}

std::vector<cloudPtrType> PCLClusteringFunction::clustering(cloudPtrType cloud)
{
    reset();

    workingCloud = cloud;

    tree->setInputCloud (workingCloud);

    ec.setClusterTolerance (clusterTolerance); // 2cm
    ec.setMinClusterSize (minClusterSize);
    ec.setMaxClusterSize (maxClusterSize);
    ec.setSearchMethod (tree);
    ec.setInputCloud (workingCloud);
    ec.extract (cluster_indices);

    QVector<pcl::PointIndices> sequence = QVector<pcl::PointIndices>::fromStdVector(cluster_indices);
    QFuture<void>future = QtConcurrent::map(sequence,
                      [&](pcl::PointIndices i){parallelClustering(i);});
    future.waitForFinished();

    /*
    std::vector<cloudPtrType> cloud_clustered;
    int j = 0;
    for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin (); it != cluster_indices.end (); ++it)
    {
        cloudPtrType cloud_cluster (new cloudType);
        for (std::vector<int>::const_iterator pit = it->indices.begin (); pit != it->indices.end (); pit++)
            cloud_cluster->points.push_back (cloud->points[*pit]);
        cloud_cluster->width = cloud_cluster->points.size ();
        cloud_cluster->height = 1;
        cloud_cluster->is_dense = true;

        cloud_clustered.push_back(cloud_cluster);

        j++;
    }
    */

    return cloud_clustered;
}

void PCLClusteringFunction::reset()
{
    cloud_clustered.clear();
    cluster_indices.clear();
    //tree.reset();
}

void PCLClusteringFunction::parallelClustering(pcl::PointIndices &aIndices)
{
    cloudPtrType cloud_cluster (new cloudType);
    for (std::vector<int>::const_iterator pit = aIndices.indices.begin (); pit != aIndices.indices.end (); pit++)
        cloud_cluster->points.push_back (workingCloud->points[*pit]); //*
    cloud_cluster->width = cloud_cluster->points.size ();
    cloud_cluster->height = 1;
    cloud_cluster->is_dense = true;

    clusteringParallelMuting.lock();
    cloud_clustered.push_back(cloud_cluster);
    clusteringParallelMuting.unlock();
}
