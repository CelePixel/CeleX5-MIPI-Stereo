#include "celex5_ros.h"

namespace celex_ros {

CelexRos::CelexRos() {}

int CelexRos::initDevice() {}

void CelexRos::grabEventData(CeleX5ProcessedData *pSensorData,
                             celex5_msgs::eventVector &msg) {
  std::vector<EventData> vecEvent;

  cv::Mat mat = cv::Mat::zeros(cv::Size(MAT_COLS, MAT_ROWS), CV_8UC1);
  int currDeveiceIndex = pSensorData->getDeviceIndex();
  msg.vectorIndex = currDeveiceIndex;
  msg.height = MAT_ROWS;
  msg.width = MAT_COLS;

  if (pSensorData->getSensorMode() == CeleX5::Event_Address_Only_Mode) {

    vecEvent = pSensorData->getEventDataVector();
    int dataSize = vecEvent.size();
    msg.vectorLength = dataSize;

    for (int i = 0; i < dataSize; i++) {
      mat.at<uchar>(MAT_ROWS - vecEvent[i].row - 1,
                    MAT_COLS - vecEvent[i].col - 1) = 255;
      event_.x = vecEvent[i].row;
      event_.y = vecEvent[i].col;
      event_.brightness = 255;
      msg.events.push_back(event_);
    }
  } else if (pSensorData->getSensorMode() == CeleX5::Event_Optical_Flow_Mode) {
    vecEvent = pSensorData->getEventDataVector();

    int dataSize = vecEvent.size();
    msg.vectorLength = dataSize;

    cv::Mat mat = cv::Mat::zeros(cv::Size(MAT_COLS, MAT_ROWS), CV_8UC1);
    for (int i = 0; i < dataSize; i++) {
      mat.at<uchar>(MAT_ROWS - vecEvent[i].row - 1,
                    MAT_COLS - vecEvent[i].col - 1) = vecEvent[i].adc;
      event_.x = vecEvent[i].row;
      event_.y = vecEvent[i].col;
      event_.brightness = vecEvent[i].adc;
      msg.events.push_back(event_);
    }
  } else {
    msg.vectorLength = 0;
    std::cout << "This mode has no event data. " << std::endl;
  }
}

CelexRos::~CelexRos() {}
}
