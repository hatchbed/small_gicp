#include <small_gicp/benchmark/benchmark_odom.hpp>

#include <small_gicp/factors/gicp_factor.hpp>
#include <small_gicp/points/point_cloud.hpp>
#include <small_gicp/ann/kdtree_omp.hpp>
#include <small_gicp/util/normal_estimation_omp.hpp>
#include <small_gicp/registration/reduction_omp.hpp>
#include <small_gicp/registration/registration.hpp>

namespace small_gicp {

class SmallGICPOnlineOdometryEstimationOMP : public OnlineOdometryEstimation {
public:
  SmallGICPOnlineOdometryEstimationOMP(const OdometryEstimationParams& params) : OnlineOdometryEstimation(params), T(Eigen::Isometry3d::Identity()) {}

  Eigen::Isometry3d estimate(const PointCloud::Ptr& points) override {
    Stopwatch sw;
    sw.start();

    auto tree = std::make_shared<KdTreeOMP<PointCloud>>(points, params.num_threads);
    estimate_covariances_omp(*points, *tree, params.num_neighbors, params.num_threads);

    if (target_points == nullptr) {
      target_points = points;
      target_tree = tree;
      return T;
    }

    Registration<GICPFactor, ParallelReductionOMP> registration;
    registration.rejector.max_dist_sq = params.max_correspondence_distance * params.max_correspondence_distance;
    registration.reduction.num_threads = params.num_threads;

    auto result = registration.align(*target_points, *points, *target_tree, Eigen::Isometry3d::Identity());

    sw.stop();
    reg_times.push(sw.msec());

    T = T * result.T_target_source;
    target_points = points;
    target_tree = tree;

    return T;
  }

  void report() override {  //
    std::cout << "registration_time_stats=" << reg_times.str() << " [msec/scan]  total_throughput=" << total_times.str() << " [msec/scan]" << std::endl;
  }

private:
  Summarizer reg_times;

  PointCloud::Ptr target_points;
  KdTreeOMP<PointCloud>::Ptr target_tree;

  Eigen::Isometry3d T;
};

static auto small_gicp_omp_registry =
  register_odometry("small_gicp_omp", [](const OdometryEstimationParams& params) { return std::make_shared<SmallGICPOnlineOdometryEstimationOMP>(params); });

}  // namespace small_gicp
