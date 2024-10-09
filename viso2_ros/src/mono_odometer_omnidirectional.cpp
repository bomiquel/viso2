#include <ros/ros.h>
#include <sensor_msgs/image_encodings.h>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/camera_subscriber.h>
#include <image_transport/image_transport.h>
#include <image_geometry/pinhole_camera_model.h>

#include <viso_mono_omnidirectional.h>

#include <viso2_ros/VisoInfo.h>

#include "odometer_base.h"
#include "odometry_params.h"


namespace viso2_ros
{

class MonoOdometerOmnidirectional : public OdometerBase
{

private:

  boost::shared_ptr<VisualOdometryMonoOmnidirectional> visual_odometer_;
  VisualOdometryMonoOmnidirectional::parameters visual_odometer_params_;

  image_transport::Subscriber camera_sub_;

  ros::Publisher info_pub_;

  bool replace_;

public:

  MonoOdometerOmnidirectional(const std::string& transport) : OdometerBase(), replace_(false)
  {
    // Read local parameters
    ros::NodeHandle local_nh("~");
    odometry_params::loadParams(local_nh, visual_odometer_params_);

    ros::NodeHandle nh;
    std::string image_topic;
    nh.param<std::string>("/mono_odometer/image", image_topic, "/image");

    image_transport::ImageTransport it(nh);
    camera_sub_ = it.subscribe(image_topic, 1, &MonoOdometerOmnidirectional::imageCallback, this);

    info_pub_ = local_nh.advertise<VisoInfo>("info", 1);
  }

protected:

  void imageCallback(const sensor_msgs::ImageConstPtr& image_msg)
  {
    ros::WallTime start_time = ros::WallTime::now();
    Matcher::visual_odometry_elapsed_time vo_elapsed_time;
 
    bool first_run = false;
    // create odometer if not exists
    if (!visual_odometer_)
    {
      first_run = true;
      visual_odometer_.reset(new VisualOdometryMonoOmnidirectional(visual_odometer_params_));
      if (image_msg->header.frame_id != "") setSensorFrameId(image_msg->header.frame_id);
      ROS_INFO_STREAM("Initialized libviso2 mono odometry "
                      "with the following parameters:" << std::endl << 
                      visual_odometer_params_);
    }

    // convert image if necessary
    uint8_t *image_data;
    int step;
    cv_bridge::CvImageConstPtr cv_ptr;
    if (image_msg->encoding == sensor_msgs::image_encodings::MONO8)
    {
      image_data = const_cast<uint8_t*>(&(image_msg->data[0]));
      step = image_msg->step;
    }
    else
    {
      cv_ptr = cv_bridge::toCvShare(image_msg, sensor_msgs::image_encodings::MONO8);
      image_data = cv_ptr->image.data;
      step = cv_ptr->image.step[0];
    }

    // run the odometer
    int32_t dims[] = {(int32_t)image_msg->width, (int32_t)image_msg->height, step};
    // on first run, only feed the odometer with first image pair without
    // retrieving data
    if (first_run)
    {
      visual_odometer_->process(image_data, dims, vo_elapsed_time);
      tf::Transform delta_transform;
      delta_transform.setIdentity();
      integrateAndPublish(delta_transform, image_msg->header.stamp);
    }
    else
    {
      bool success = visual_odometer_->process(image_data, dims, vo_elapsed_time);
      if(success)
      {
        replace_ = false;
        Matrix camera_motion = Matrix::inv(visual_odometer_->getMotion());
        ROS_DEBUG("Found %i matches with %i inliers.", 
                  visual_odometer_->getNumberOfMatches(),
                  visual_odometer_->getNumberOfInliers());
        ROS_DEBUG_STREAM("libviso2 returned the following motion:\n" << camera_motion);

        tf::Matrix3x3 rot_mat(
          camera_motion.val[0][0], camera_motion.val[0][1], camera_motion.val[0][2],
          camera_motion.val[1][0], camera_motion.val[1][1], camera_motion.val[1][2],
          camera_motion.val[2][0], camera_motion.val[2][1], camera_motion.val[2][2]);
        tf::Vector3 t(camera_motion.val[0][3], camera_motion.val[1][3], camera_motion.val[2][3]);
        tf::Transform delta_transform(rot_mat, t);

        integrateAndPublish(delta_transform, image_msg->header.stamp);
      }
      else
      {
        ROS_DEBUG("Call to VisualOdometryMono::process() failed. Assuming motion too small.");
        replace_ = true;
        tf::Transform delta_transform;
        delta_transform.setIdentity();
        integrateAndPublish(delta_transform, image_msg->header.stamp);
      }

      // create and publish viso2 info msg
      VisoInfo info_msg;
      info_msg.header.stamp = image_msg->header.stamp;
      info_msg.got_lost = !success;
      info_msg.change_reference_frame = false;
      info_msg.num_matches = visual_odometer_->getNumberOfMatches();
      info_msg.num_inliers = visual_odometer_->getNumberOfInliers();
      info_msg.feature_detection_runtime = vo_elapsed_time.feature_detection;
      info_msg.feature_matching_runtime = vo_elapsed_time.feature_matching;
      info_msg.motion_estimation_runtime = vo_elapsed_time.motion_estimation;
      ros::WallDuration time_elapsed = ros::WallTime::now() - start_time;
      info_msg.vo_runtime = time_elapsed.toSec();
      info_pub_.publish(info_msg);
    }
  }
};

} // end of namespace


int main(int argc, char **argv)
{
  ros::init(argc, argv, "mono_odometer");
  if (ros::names::remap("image").find("rect") == std::string::npos) {
    ROS_WARN("mono_odometer needs rectified input images. The used image "
             "topic is '%s'. Are you sure the images are rectified?",
             ros::names::remap("image").c_str());
  }

  std::string transport = argc > 1 ? argv[1] : "raw";
  viso2_ros::MonoOdometerOmnidirectional odometer(transport);
  
  ros::spin();
  return 0;
}
