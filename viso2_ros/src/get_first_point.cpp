#include <fstream>
#include <ros/ros.h>
#include <ros/console.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/Range.h>
#include <sensor_msgs/Image.h>
#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <image_transport/subscriber_filter.h>
#include <message_filters/sync_policies/approximate_time.h>

using namespace std;

double max_altitude_ ;
string outfile_ ;


void callback(const nav_msgs::Odometry::ConstPtr& map_msg, const sensor_msgs::Image::ConstPtr& img_msg, const sensor_msgs::Range::ConstPtr& altitude_msg)
{
    ROS_INFO("HELLO THERE") ;

    if (altitude_msg->range < max_altitude_)
    {
        ros::Time timestamp = map_msg->header.stamp ;
        double tx = map_msg->pose.pose.position.x ;
        double ty = map_msg->pose.pose.position.y ;
        double tz = altitude_msg->range ;
        double qx = map_msg->pose.pose.orientation.x ;
        double qy = map_msg->pose.pose.orientation.y ;
        double qz = map_msg->pose.pose.orientation.z ;
        double qw = map_msg->pose.pose.orientation.w ;

        fstream fout ;
        fout.open(outfile_, ios::out | ios::app) ;

        fout << fixed << setprecision(15) 
             << timestamp << ";"
             << tx << ";" << ty << ";" << tz << ";"
             << qx << ";" << qy << ";" << qz << ";" << qw << "\n" ;

        fout.close() ;

        ROS_INFO_STREAM("LO TENGOOOOOOOO") ;

    }

}


int main(int argc, char** argv){
 
    ros::init(argc, argv, "obtain_first_point") ;

    ros::NodeHandle nh ;
    ros::NodeHandle nhp("~") ;

    nhp.param("outfile", outfile_, string("")) ;
    nhp.param("max_altitude", max_altitude_, 3.5) ;

    ROS_INFO_STREAM("Initialized obtain_first_point "
                    "with the following parameters:" << endl <<
                    "  outfile = " << outfile_ << endl <<
                    "  max_altitude = " << max_altitude_            
                    );

    image_transport::ImageTransport it(nh);
    image_transport::SubscriberFilter image_sub;
    message_filters::Subscriber<nav_msgs::Odometry> map_sub;
    message_filters::Subscriber<sensor_msgs::Range> altitude_sub;

    typedef message_filters::sync_policies::ApproximateTime<nav_msgs::Odometry, sensor_msgs::Image, sensor_msgs::Range> SyncPolicy;
    typedef message_filters::Synchronizer<SyncPolicy> Sync;

    boost::shared_ptr<Sync> sync ;
    map_sub     .subscribe(nh, "odom", 1);
    image_sub   .subscribe(it, "image", 1);
    altitude_sub.subscribe(nh, "altitude", 1);
    sync.reset(new Sync(SyncPolicy(20), map_sub, image_sub, altitude_sub)) ;
    sync->registerCallback(bind(&callback, _1, _2, _3)) ;

    ros::spin() ;
    ros::shutdown();

    return 0 ;
}