/*
    Copyright 2012-2016 Benjamin Vedder	benjamin@vedder.se

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#include "packetinterface.h"
#include "utility.h"
#include <QDebug>
#include <math.h>
#include <QEventLoop>

namespace {
// CRC Table
const unsigned short crc16_tab[] = { 0x0000, 0x1021, 0x2042, 0x3063, 0x4084,
        0x50a5, 0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad,
        0xe1ce, 0xf1ef, 0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7,
        0x62d6, 0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a,
        0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d, 0x3653, 0x2672,
        0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719,
        0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5, 0x6886, 0x78a7,
        0x0840, 0x1861, 0x2802, 0x3823, 0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948,
        0x9969, 0xa90a, 0xb92b, 0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50,
        0x3a33, 0x2a12, 0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b,
        0xab1a, 0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97,
        0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70, 0xff9f, 0xefbe,
        0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca,
        0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3,
        0x5004, 0x4025, 0x7046, 0x6067, 0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d,
        0xd31c, 0xe37f, 0xf35e, 0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214,
        0x6277, 0x7256, 0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c,
        0xc50d, 0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3,
        0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634, 0xd94c, 0xc96d,
        0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806,
        0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3, 0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e,
        0x8bf9, 0x9bd8, 0xabbb, 0xbb9a, 0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1,
        0x1ad0, 0x2ab3, 0x3a92, 0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b,
        0x9de8, 0x8dc9, 0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0,
        0x0cc1, 0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0 };
}

PacketInterface::PacketInterface(QObject *parent) :
    QObject(parent)
{
    mSendBuffer = new quint8[mMaxBufferLen + 20];

    mRxState = 0;
    mRxTimer = 0;

    // Packet state
    mPayloadLength = 0;
    mRxDataPtr = 0;
    mCrcLow = 0;
    mCrcHigh = 0;

    mTimer = new QTimer(this);
    mTimer->setInterval(10);
    mTimer->start();

    mHostAddress = QHostAddress("0.0.0.0");
    mUdpPort = 0;
    mUdpSocket = new QUdpSocket(this);

    connect(mUdpSocket, SIGNAL(readyRead()),
            this, SLOT(readPendingDatagrams()));
    connect(mTimer, SIGNAL(timeout()), this, SLOT(timerSlot()));
}

PacketInterface::~PacketInterface()
{
    delete mSendBuffer;
}

void PacketInterface::processData(QByteArray &data)
{
    unsigned char rx_data;
    const int rx_timeout = 50;

    for(int i = 0;i < data.length();i++) {
        rx_data = data[i];

        switch (mRxState) {
        case 0:
            if (rx_data == 2) {
                mRxState += 2;
                mRxTimer = rx_timeout;
                mRxDataPtr = 0;
                mPayloadLength = 0;
            } else if (rx_data == 3) {
                mRxState++;
                mRxTimer = rx_timeout;
                mRxDataPtr = 0;
                mPayloadLength = 0;
            } else {
                mRxState = 0;
            }
            break;

        case 1:
            mPayloadLength = (unsigned int)rx_data << 8;
            mRxState++;
            mRxTimer = rx_timeout;
            break;

        case 2:
            mPayloadLength |= (unsigned int)rx_data;
            if (mPayloadLength <= mMaxBufferLen && mPayloadLength > 0) {
                mRxState++;
                mRxTimer = rx_timeout;
            } else {
                mRxState = 0;
            }
            break;

        case 3:
            mRxBuffer[mRxDataPtr++] = rx_data;
            if (mRxDataPtr == mPayloadLength) {
                mRxState++;
            }
            mRxTimer = rx_timeout;
            break;

        case 4:
            mCrcHigh = rx_data;
            mRxState++;
            mRxTimer = rx_timeout;
            break;

        case 5:
            mCrcLow = rx_data;
            mRxState++;
            mRxTimer = rx_timeout;
            break;

        case 6:
            if (rx_data == 3) {
                if (crc16(mRxBuffer, mPayloadLength) ==
                        ((unsigned short)mCrcHigh << 8 | (unsigned short)mCrcLow)) {
                    // Packet received!
                    processPacket(mRxBuffer, mPayloadLength);
                }
            }

            mRxState = 0;
            break;

        default:
            mRxState = 0;
            break;
        }
    }
}

void PacketInterface::timerSlot()
{
    if (mRxTimer) {
        mRxTimer--;
    } else {
        mRxState = 0;
    }
}

void PacketInterface::readPendingDatagrams()
{
    while (mUdpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(mUdpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        mUdpSocket->readDatagram(datagram.data(), datagram.size(),
                                &sender, &senderPort);

        processPacket((unsigned char*)datagram.data(), datagram.length());
    }
}

unsigned short PacketInterface::crc16(const unsigned char *buf, unsigned int len)
{
    unsigned int i;
    unsigned short cksum = 0;
    for (i = 0; i < len; i++) {
        cksum = crc16_tab[(((cksum >> 8) ^ *buf++) & 0xFF)] ^ (cksum << 8);
    }
    return cksum;
}

bool PacketInterface::sendPacket(const unsigned char *data, unsigned int len_packet)
{    
    static unsigned char buffer[mMaxBufferLen];
    unsigned int ind = 0;

    // If the IP is valid, send the packet over UDP
    if (QString::compare(mHostAddress.toString(), "0.0.0.0") != 0) {
        memcpy(buffer + ind, data, len_packet);
        ind += len_packet;

        mUdpSocket->writeDatagram(QByteArray::fromRawData((const char*)buffer, ind), mHostAddress, mUdpPort);
        return true;
    }

    int len_tot = len_packet;
    unsigned int data_offs = 0;

    if (len_tot <= 256) {
        buffer[ind++] = 2;
        buffer[ind++] = len_tot;
        data_offs = 2;
    } else {
        buffer[ind++] = 3;
        buffer[ind++] = len_tot >> 8;
        buffer[ind++] = len_tot & 0xFF;
        data_offs = 3;
    }

    memcpy(buffer + ind, data, len_packet);
    ind += len_packet;

    unsigned short crc = crc16(buffer + data_offs, len_tot);
    buffer[ind++] = crc >> 8;
    buffer[ind++] = crc;
    buffer[ind++] = 3;

    QByteArray sendData = QByteArray::fromRawData((char*)buffer, ind);

    emit dataToSend(sendData);

    return true;
}

bool PacketInterface::sendPacket(QByteArray data)
{
    return sendPacket((const unsigned char*)data.data(), data.size());
}

/**
 * @brief PacketInterface::sendPacketAck
 * Send packet and wait for acknoledgement.
 *
 * @param data
 * The data to be sent.
 *
 * @param len_packet
 * Size of the data.
 *
 * @param retries
 * The maximum number of retries before giving up.
 *
 * @param timeoutMs
 * Time to wait before trying again.
 *
 * @return
 * True for success, false otherwise.
 */
bool PacketInterface::sendPacketAck(const unsigned char *data, unsigned int len_packet,
                                    int retries, int timeoutMs)
{
    static bool waiting = false;

    if (waiting) {
        qDebug() << "Already waiting for packet";
        return false;
    }

    waiting = true;

    unsigned char *buffer = new unsigned char[mMaxBufferLen];
    bool ok = false;
    memcpy(buffer, data, len_packet);

    for (int i = 0;i < retries;i++) {
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.start(timeoutMs);
        connect(this, SIGNAL(ackReceived(quint8, CMD_PACKET, QString)), &loop, SLOT(quit()));
        connect(&timeoutTimer, SIGNAL(timeout()), &loop, SLOT(quit()));

        sendPacket(buffer, len_packet);
        loop.exec();

        if (timeoutTimer.isActive()) {
            ok = true;
            break;
        }

        qDebug() << "Retrying to send packet...";
    }

    waiting = false;
    delete buffer;
    return ok;
}

void PacketInterface::processPacket(const unsigned char *data, int len)
{
    unsigned char id = data[0];
    data++;
    len--;

    CMD_PACKET cmd = (CMD_PACKET)(quint8)data[0];
    data++;
    len--;

    switch (cmd) {
    case CMD_PRINTF: {
        QByteArray tmpArray = QByteArray::fromRawData((char*)data, len);
        tmpArray[len] = '\0';
        emit printReceived(id, QString::fromLatin1(tmpArray));
    } break;

    case CMD_GET_STATE: {
        CAR_STATE state;
        int32_t ind = 0;

        state.fw_major = data[ind++];
        state.fw_minor = data[ind++];
        state.roll = utility::buffer_get_double32(data, 1e6, &ind);
        state.pitch = utility::buffer_get_double32(data, 1e6, &ind);
        state.yaw = utility::buffer_get_double32(data, 1e6, &ind);
        state.accel[0] = utility::buffer_get_double32(data, 1e6, &ind);
        state.accel[1] = utility::buffer_get_double32(data, 1e6, &ind);
        state.accel[2] = utility::buffer_get_double32(data, 1e6, &ind);
        state.gyro[0] = utility::buffer_get_double32(data, 1e6, &ind);
        state.gyro[1] = utility::buffer_get_double32(data, 1e6, &ind);
        state.gyro[2] = utility::buffer_get_double32(data, 1e6, &ind);
        state.mag[0] = utility::buffer_get_double32(data, 1e6, &ind);
        state.mag[1] = utility::buffer_get_double32(data, 1e6, &ind);
        state.mag[2] = utility::buffer_get_double32(data, 1e6, &ind);
        state.q[0] = utility::buffer_get_double32(data, 1e8, &ind);
        state.q[1] = utility::buffer_get_double32(data, 1e8, &ind);
        state.q[2] = utility::buffer_get_double32(data, 1e8, &ind);
        state.q[3] = utility::buffer_get_double32(data, 1e8, &ind);
        state.px = utility::buffer_get_double32(data, 1e4, &ind);
        state.py = utility::buffer_get_double32(data, 1e4, &ind);
        state.speed = utility::buffer_get_double32(data, 1e6, &ind);
        state.vin = utility::buffer_get_double32(data, 1e6, &ind);
        state.temp_fet = utility::buffer_get_double32(data, 1e6, &ind);
        state.mc_fault = (mc_fault_code)data[ind++];
        emit stateReceived(id, state);
    } break;

    case CMD_VESC_FWD:
        emit vescFwdReceived(id, QByteArray::fromRawData((char*)data, len));
        break;

    case CMD_SEND_RTCM_USB: {
        QByteArray tmpArray((char*)data, len);
        emit rtcmUsbReceived(id, tmpArray);
    } break;

    case CMD_SEND_NMEA_RADIO: {
        QByteArray tmpArray((char*)data, len);
        emit nmeaRadioReceived(id, tmpArray);
    } break;

    case CMD_GET_MAIN_CONFIG:
    case CMD_GET_MAIN_CONFIG_DEFAULT: {
        MAIN_CONFIG conf;

        int32_t ind = 0;
        conf.mag_comp = data[ind++];
        conf.yaw_imu_gain = utility::buffer_get_double32(data, 1e6, &ind);

        conf.mag_cal_cx = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_cy = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_cz = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_xx = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_xy = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_xz = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_yx = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_yy = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_yz = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_zx = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_zy = utility::buffer_get_double32(data, 1e6, &ind);
        conf.mag_cal_zz = utility::buffer_get_double32(data, 1e6, &ind);

        conf.gear_ratio = utility::buffer_get_double32(data, 1e6, &ind);
        conf.wheel_diam = utility::buffer_get_double32(data, 1e6, &ind);
        conf.motor_poles = utility::buffer_get_double32(data, 1e6, &ind);
        conf.steering_max_angle_rad = utility::buffer_get_double32(data, 1e6, &ind);
        conf.steering_center = utility::buffer_get_double32(data, 1e6, &ind);
        conf.steering_left = utility::buffer_get_double32(data, 1e6, &ind);
        conf.steering_right = utility::buffer_get_double32(data, 1e6, &ind);
        conf.steering_ramp_time = utility::buffer_get_double32(data, 1e6, &ind);
        conf.axis_distance = utility::buffer_get_double32(data, 1e6, &ind);

        emit configurationReceived(id, conf);
    } break;

        // Acks
    case CMD_AP_ADD_POINTS:
        emit ackReceived(id, cmd, "CMD_AP_ADD_POINTS");
        break;
    case CMD_AP_REMOVE_LAST_POINT:
        emit ackReceived(id, cmd, "CMD_AP_REMOVE_LAST_POINT");
        break;
    case CMD_AP_CLEAR_POINTS:
        emit ackReceived(id, cmd, "CMD_AP_CLEAR_POINTS");
        break;
    case CMD_AP_SET_ACTIVE:
        emit ackReceived(id, cmd, "CMD_AP_SET_ACTIVE");
        break;
    case CMD_SET_MAIN_CONFIG:
        emit ackReceived(id, cmd, "CMD_SET_MAIN_CONFIG");
        break;

    default:
        break;
    }
}

void PacketInterface::startUdpConnection(QHostAddress ip, int port)
{
    mHostAddress = ip;
    mUdpPort = port;
    mUdpSocket->close();
    mUdpSocket->bind(QHostAddress::Any, mUdpPort + 1);
}

void PacketInterface::stopUdpConnection()
{
    mHostAddress = QHostAddress("0.0.0.0");
    mUdpPort = 0;
    mUdpSocket->close();
}

bool PacketInterface::isUdpConnected()
{
    return QString::compare(mHostAddress.toString(), "0.0.0.0") != 0;
}

void PacketInterface::getState(quint8 id)
{
    QByteArray packet;
    packet.append(id);
    packet.append(CMD_GET_STATE);
    sendPacket(packet);
}

bool PacketInterface::setRoutePoints(quint8 id, QList<LocPoint> points, int retries)
{
    qint32 send_index = 0;
    mSendBuffer[send_index++] = id;
    mSendBuffer[send_index++] = CMD_AP_ADD_POINTS;

    for (int i = 0;i < points.size();i++) {
        LocPoint *p = &points[i];
        utility::buffer_append_double32(mSendBuffer, p->getX(), 1e4, &send_index);
        utility::buffer_append_double32(mSendBuffer, p->getY(), 1e4, &send_index);
        utility::buffer_append_double32(mSendBuffer, p->getSpeed(), 1e6, &send_index);
    }

    return sendPacketAck(mSendBuffer, send_index, retries);
}

bool PacketInterface::removeLastRoutePoint(quint8 id, int retries)
{
    qint32 send_index = 0;
    mSendBuffer[send_index++] = id;
    mSendBuffer[send_index++] = CMD_AP_REMOVE_LAST_POINT;

    return sendPacketAck(mSendBuffer, send_index, retries);
}

bool PacketInterface::clearRoute(quint8 id, int retries)
{
    qint32 send_index = 0;
    mSendBuffer[send_index++] = id;
    mSendBuffer[send_index++] = CMD_AP_CLEAR_POINTS;

    return sendPacketAck(mSendBuffer, send_index, retries);
}

bool PacketInterface::setApActive(quint8 id, bool active, int retries)
{
    qint32 send_index = 0;
    mSendBuffer[send_index++] = id;
    mSendBuffer[send_index++] = CMD_AP_SET_ACTIVE;
    mSendBuffer[send_index++] = active ? 1 : 0;

    return sendPacketAck(mSendBuffer, send_index, retries);
}

bool PacketInterface::setConfiguration(quint8 id, MAIN_CONFIG &conf, int retries)
{
    qint32 send_index = 0;
    mSendBuffer[send_index++] = id;
    mSendBuffer[send_index++] = CMD_SET_MAIN_CONFIG;

    mSendBuffer[send_index++] = conf.mag_comp;
    utility::buffer_append_double32(mSendBuffer, conf.yaw_imu_gain, 1e6, &send_index);

    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_cx, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_cy, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_cz, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_xx, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_xy, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_xz, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_yx, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_yy, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_yz, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_zx, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_zy, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.mag_cal_zz, 1e6, &send_index);

    utility::buffer_append_double32(mSendBuffer, conf.gear_ratio, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.wheel_diam, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.motor_poles, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.steering_max_angle_rad, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.steering_center, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.steering_left, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.steering_right, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.steering_ramp_time, 1e6, &send_index);
    utility::buffer_append_double32(mSendBuffer, conf.axis_distance, 1e6, &send_index);

    return sendPacketAck(mSendBuffer, send_index, retries, 2000);
}

void PacketInterface::sendTerminalCmd(quint8 id, QString cmd)
{
    QByteArray packet;
    packet.clear();
    packet.append(id);
    packet.append((char)CMD_TERMINAL_CMD);
    packet.append(cmd.toLatin1());
    sendPacket(packet);
}

void PacketInterface::forwardVesc(quint8 id, QByteArray data)
{
    QByteArray packet;
    packet.clear();
    packet.append(id);
    packet.append((char)CMD_VESC_FWD);
    packet.append(data);
    sendPacket(packet);
}

void PacketInterface::setRcControlCurrent(quint8 id, double duty, double steering)
{
    qint32 send_index = 0;
    mSendBuffer[send_index++] = id;
    mSendBuffer[send_index++] = CMD_RC_CONTROL;
    mSendBuffer[send_index++] = RC_MODE_CURRENT;
    utility::buffer_append_double32(mSendBuffer, duty, 1e4, &send_index);
    utility::buffer_append_double32(mSendBuffer, steering, 1e6, &send_index);
    sendPacket(mSendBuffer, send_index);
}

void PacketInterface::setRcControlDuty(quint8 id, double duty, double steering)
{
    qint32 send_index = 0;
    mSendBuffer[send_index++] = id;
    mSendBuffer[send_index++] = CMD_RC_CONTROL;
    mSendBuffer[send_index++] = RC_MODE_DUTY;
    utility::buffer_append_double32(mSendBuffer, duty, 1e4, &send_index);
    utility::buffer_append_double32(mSendBuffer, steering, 1e6, &send_index);
    sendPacket(mSendBuffer, send_index);
}

void PacketInterface::setPos(quint8 id, double x, double y, double angle)
{
    qint32 send_index = 0;
    mSendBuffer[send_index++] = id;
    mSendBuffer[send_index++] = CMD_SET_POS;
    utility::buffer_append_double32(mSendBuffer, x, 1e4, &send_index);
    utility::buffer_append_double32(mSendBuffer, y, 1e4, &send_index);
    utility::buffer_append_double32(mSendBuffer, angle, 1e6, &send_index);
    sendPacket(mSendBuffer, send_index);
}

void PacketInterface::setServoDirect(quint8 id, double value)
{
    qint32 send_index = 0;
    mSendBuffer[send_index++] = id;
    mSendBuffer[send_index++] = CMD_SET_SERVO_DIRECT;
    utility::buffer_append_double32(mSendBuffer, value, 1e6, &send_index);
    sendPacket(mSendBuffer, send_index);
}

void PacketInterface::sendRtcmUsb(quint8 id, QByteArray rtcm_msg)
{
    QByteArray packet;
    packet.clear();
    packet.append(id);
    packet.append((char)CMD_SEND_RTCM_USB);
    packet.append(rtcm_msg);
    sendPacket(packet);
}

void PacketInterface::sendNmeaRadio(quint8 id, QByteArray nmea_msg)
{
    QByteArray packet;
    packet.clear();
    packet.append(id);
    packet.append((char)CMD_SEND_NMEA_RADIO);
    packet.append(nmea_msg);
    sendPacket(packet);
}

void PacketInterface::getConfiguration(quint8 id)
{
    QByteArray packet;
    packet.clear();
    packet.append(id);
    packet.append((char)CMD_GET_MAIN_CONFIG);
    sendPacket(packet);
}

void PacketInterface::getDefaultConfiguration(quint8 id)
{
    QByteArray packet;
    packet.clear();
    packet.append(id);
    packet.append((char)CMD_GET_MAIN_CONFIG_DEFAULT);
    sendPacket(packet);
}
