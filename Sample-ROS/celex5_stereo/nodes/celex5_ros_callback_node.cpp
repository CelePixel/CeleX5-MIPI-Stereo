#include "celex5_ros.h"

static const std::string OPENCV_WINDOW = "Image window";
static const std::string OPENCV_WINDOW_1 = "Image window_1";
namespace celex_ros_callback {
class CelexRosCallBackNode : public CeleX5DataManager {
public:
  // private ROS node handle
  ros::NodeHandle node_;

  ros::Publisher data_pub_[2];
  ros::Publisher image_pub_[2];
  ros::Subscriber data_sub_[2];

  // custom celex5 message type
  celex5_msgs::eventVector event_vector_;

  // parameters
  std::string celex_mode_, event_pic_type_;
  int threshold_, clock_rate_;

  CX5SensorDataServer *m_pServer_;
  CeleX5 *celex_;
  celex_ros::CelexRos celexRos_;

  CelexRosCallBackNode(CX5SensorDataServer *pServer) : node_("~") {
    m_pServer_ = pServer;
    m_pServer_->registerData(this, CeleX5DataManager::CeleX_Frame_Data);

    image_pub_[0] = node_.advertise<sensor_msgs::Image>("/imgshow", 1);
    image_pub_[1] = node_.advertise<sensor_msgs::Image>("/imgshow1", 1);

    // advertise custom celex5 data and subscribe the data
    data_pub_[0] = node_.advertise<celex5_msgs::eventVector>("celex5", 1);
    data_pub_[1] = node_.advertise<celex5_msgs::eventVector>("celex5_1", 1);

    data_sub_[0] = node_.subscribe(
        "celex5", 1, &CelexRosCallBackNode::celexDataCallback, this);

    data_sub_[1] = node_.subscribe(
        "celex5_1", 1, &CelexRosCallBackNode::celexDataCallback1, this);

    // grab the parameters
    node_.param<std::string>("celex_mode", celex_mode_,
                             "Event_Address_Only_Mode");
    node_.param<std::string>("event_pic_type", event_pic_type_,
                             "EventBinaryPic");

    node_.param<int>("threshold", threshold_, 170);   // 0-1024
    node_.param<int>("clock_rate", clock_rate_, 100); // 0-100

    // create the display windows
    cv::namedWindow(OPENCV_WINDOW);
    cv::namedWindow(OPENCV_WINDOW_1);
  }

  ~CelexRosCallBackNode() {
    m_pServer_->unregisterData(this, CeleX5DataManager::CeleX_Frame_Data);
    delete celex_;
    cv::destroyWindow(OPENCV_WINDOW);
    cv::destroyWindow(OPENCV_WINDOW_1);
  }

  // overrides the update operation
  virtual void onFrameDataUpdated(CeleX5ProcessedData *pSensorData);

  // subscribe callback function
  void celexDataCallback(const celex5_msgs::eventVector &msg);
  void celexDataCallback1(const celex5_msgs::eventVector &msg);

  void setCeleX5(CeleX5 *pcelex);
  bool spin();
};

// callback for data_sub_[0]
void CelexRosCallBackNode::celexDataCallback(
    const celex5_msgs::eventVector &msg) {
  ROS_INFO("I heard celex5 data %d size: [%d]", msg.vectorIndex,
           msg.vectorLength);

  // display the image
  if (msg.vectorLength > 0) {
    cv::Mat mat = cv::Mat::zeros(cv::Size(MAT_COLS, MAT_ROWS), CV_8UC1);
    for (int i = 0; i < msg.vectorLength; i++) {
      mat.at<uchar>(MAT_ROWS - msg.events[i].x - 1,
                    MAT_COLS - msg.events[i].y - 1) = msg.events[i].brightness;
    }
    if (msg.vectorIndex == 0)
      cv::imshow(OPENCV_WINDOW, mat);
    else
      cv::imshow(OPENCV_WINDOW_1, mat);
    cv::waitKey(1);
  }
}

// callback for data_sub_[1]
void CelexRosCallBackNode::celexDataCallback1(
    const celex5_msgs::eventVector &msg) {
  ROS_INFO("I heard celex5 data %d size: [%d]", msg.vectorIndex,
           msg.vectorLength);

  // display the image
  if (msg.vectorLength > 0) {
    cv::Mat mat = cv::Mat::zeros(cv::Size(MAT_COLS, MAT_ROWS), CV_8UC1);
    for (int i = 0; i < msg.vectorLength; i++) {
      mat.at<uchar>(MAT_ROWS - msg.events[i].x - 1,
                    MAT_COLS - msg.events[i].y - 1) = msg.events[i].brightness;
    }
    if (msg.vectorIndex == 0)
      cv::imshow(OPENCV_WINDOW, mat);
    else
      cv::imshow(OPENCV_WINDOW_1, mat);
    cv::waitKey(1);
  }
}

// set the celex5 and some settings
void CelexRosCallBackNode::setCeleX5(CeleX5 *pcelex) {
  celex_ = pcelex;

  celex_->setThreshold(threshold_);
  celex_->setThreshold(threshold_, 1);

  CeleX5::CeleX5Mode mode;
  if (celex_mode_ == "Event_Address_Only_Mode")
    mode = CeleX5::Event_Address_Only_Mode;
  else if (celex_mode_ == "Event_Optical_Flow_Mode")
    mode = CeleX5::Event_Optical_Flow_Mode;
  celex_->setSensorFixedMode(mode);
  celex_->setSensorFixedMode(mode, 1);
}

// the callback update function
void CelexRosCallBackNode::onFrameDataUpdated(
    CeleX5ProcessedData *pSensorData) {

  // get sensor image and publish it
  cv::Mat image =
      cv::Mat(800, 1280, CV_8UC1,
              pSensorData->getEventPicBuffer(CeleX5::EventBinaryPic));
  sensor_msgs::ImagePtr msg =
      cv_bridge::CvImage(std_msgs::Header(), "mono8", image).toImageMsg();
  if (pSensorData->getDeviceIndex() == 0)
    image_pub_[0].publish(msg);
  else
    image_pub_[1].publish(msg);

  // get raw event data and publish it
  celexRos_.grabEventData(pSensorData, event_vector_);
  if (event_vector_.vectorIndex == 0)
    data_pub_[0].publish(event_vector_);
  else
    data_pub_[1].publish(event_vector_);
  event_vector_.events.clear();
}

bool CelexRosCallBackNode::spin() {
  ros::Rate loop_rate(30);
  while (node_.ok()) {
    ros::spinOnce();
    loop_rate.sleep();
  }
  return true;
}
}

int main(int argc, char **argv) {
  ros::init(argc, argv, "celex_ros_callback");

  CeleX5 *pCelex_;
  pCelex_ = new CeleX5;
  if (NULL == pCelex_)
    return 0;
  pCelex_->openSensor(CeleX5::CeleX5_MIPI);

  celex_ros_callback::CelexRosCallBackNode *cr =
      new celex_ros_callback::CelexRosCallBackNode(
          pCelex_->getSensorDataServer(0));

  cr->setCeleX5(pCelex_);

  celex_ros_callback::CelexRosCallBackNode *cr1 =
      new celex_ros_callback::CelexRosCallBackNode(
          pCelex_->getSensorDataServer(1));

  cr->spin();

  return EXIT_SUCCESS;
}
