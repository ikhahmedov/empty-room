#ifndef _PAIRWISE_H
#define _PAIRWISE_H

#include <Eigen/Eigen>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <vector>

/**
 * Calculates which planes in source map to which planes in tgt. Returns the
 * number of correspondences found
 */
int findPlaneCorrespondences(
        pcl::PointCloud<pcl::PointXYZ>::ConstPtr src,
        pcl::PointCloud<pcl::PointXYZ>::ConstPtr tgt,
        std::vector<Eigen::Vector4d>& srcplanes, std::vector<int>& srcids,
        std::vector<Eigen::Vector4d>& tgtplanes, std::vector<int>& tgtids,
        std::vector<int>& planecorrespondences);

/**
 * Returns the transformation mapping src to tgt
 * Assumes that the planes in tgt and src have already been computed in the
 * appropriate planes and ids vectors, or it will calculate them.
 *
 * Does NOT transform src or srcplanes into tgt's coordinate system.
 */
Eigen::Matrix4d align(
        pcl::PointCloud<pcl::PointXYZ>::ConstPtr src,
        pcl::PointCloud<pcl::PointXYZ>::ConstPtr tgt,
        std::vector<Eigen::Vector4d>& srcplanes, std::vector<int>& srcids,
        std::vector<Eigen::Vector4d>& tgtplanes, std::vector<int>& tgtids);

/**
 * Applies the transform to the plane in Ax + By + Cz + D = 0 form. Assumes the
 * normal (A,B,C) is normalized.
 */
Eigen::Vector4d transformPlane(Eigen::Vector4d plane, Eigen::Matrix4d transform);
#endif
