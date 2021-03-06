/*
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include <QtDBus/QDBusConnection>

#include "counter.h"
#include "networkmanager.h"


Counter::Counter(QObject *parent) :
    QObject(parent),
    m_manager(NetworkManagerFactory::createInstance()),
    bytesInHome(0),
    bytesOutHome(0),
    secondsOnlineHome(0),
    bytesInRoaming(0),
    bytesOutRoaming(0),
    secondsOnlineRoaming(0),
    roamingEnabled(false),
    currentInterval(1),
    currentAccuracy(1024),
    isRunning(false),
    shouldBeRunning(false)
{
    QTime time = QTime::currentTime();
    qsrand((uint)time.msec());
    int randomValue = qrand();
    //this needs to be unique so we can use more than one at a time with different processes
    counterPath = "/ConnectivityCounter" + QString::number(randomValue);

    connect(m_manager, SIGNAL(availabilityChanged(bool)),
            this, SLOT(updateMgrAvailability(bool)));
    if (QDBusConnection::systemBus().interface()->isServiceRegistered("net.connman"))
        updateMgrAvailability(true);
}

Counter::~Counter()
{
    m_manager->unregisterAgent(QString(counterPath));
}

void Counter::serviceUsage(const QString &servicePath, const QVariantMap &counters,  bool roaming)
{
    Q_EMIT counterChanged(servicePath, counters, roaming);

    if (roaming != roamingEnabled) {
        roamingEnabled = roaming;
        Q_EMIT roamingChanged(roaming);
    }

    quint64 rxbytes = counters["RX.Bytes"].toULongLong();
    quint64 txbytes = counters["TX.Bytes"].toULongLong();
    quint32 time = counters["Time"].toUInt();

    if (roaming) {
        if (rxbytes != 0) {
            bytesInRoaming = rxbytes;
        }
        if (txbytes != 0) {
            bytesOutRoaming = txbytes;
        }
        if (time != 0) {
            secondsOnlineRoaming = time;
        }
    } else {
        if (rxbytes != 0) {
            bytesInHome = rxbytes;
        }
        if (txbytes != 0) {
            bytesOutHome = txbytes;
        }
        if (time != 0) {
            secondsOnlineHome = time;
        }
    }

    if (rxbytes != 0)
        Q_EMIT bytesReceivedChanged(rxbytes);
    if (txbytes != 0)
        Q_EMIT bytesTransmittedChanged(txbytes);
    if (time != 0)
        Q_EMIT secondsOnlineChanged(time);
}

void Counter::release()
{
}

bool Counter::roaming() const
{
    return roamingEnabled;
}

quint64 Counter::bytesReceived() const
{
    if (roamingEnabled) {
        return bytesInRoaming;
    } else {
        return bytesInHome;
    }
}

quint64 Counter::bytesTransmitted() const
{
    if (roamingEnabled) {
        return bytesOutRoaming;
    } else {
        return bytesOutHome;
    }
}

quint32 Counter::secondsOnline() const
{
    if (roamingEnabled) {
        return secondsOnlineRoaming;
    } else {
        return secondsOnlineHome;
    }
}

/*
 *The accuracy value is in kilo-bytes. It defines
            the update threshold.

Changing the accuracy will reset the counters, as it will
need to be re registered from the manager.
*/
void Counter::setAccuracy(quint32 accuracy)
{
    currentAccuracy = accuracy;
    reRegister();
    Q_EMIT accuracyChanged(accuracy);
}

quint32 Counter::accuracy() const
{
    return currentAccuracy;
}

/*
 *The interval value is in seconds.
Changing the accuracy will reset the counters, as it will
need to be re registered from the manager.
*/
void Counter::setInterval(quint32 interval)
{
    currentInterval = interval;
    reRegister();
    Q_EMIT intervalChanged(interval);
}

quint32 Counter::interval() const
{
    return currentInterval;
}

void Counter::reRegister()
{
    if (m_manager->isAvailable()) {
        m_manager->unregisterCounter(QString(counterPath));
        m_manager->registerCounter(QString(counterPath),currentAccuracy,currentInterval);
    }
}

void Counter::setRunning(bool on)
{
    shouldBeRunning = on;
    if (on) {
        if (m_manager->isAvailable()) {
            m_manager->registerCounter(QString(counterPath),currentAccuracy,currentInterval);
            isRunning = true;
            Q_EMIT runningChanged(isRunning);
        }
    } else {
        if (m_manager->isAvailable()) {
            m_manager->unregisterCounter(QString(counterPath));
            isRunning = false;
            Q_EMIT runningChanged(isRunning);
        }
    }
}

bool Counter::running() const
{
    return isRunning;
}

void Counter::updateMgrAvailability(bool available)
{
    if (available) {
        new CounterAdaptor(this);
        if (!QDBusConnection::systemBus().registerObject(counterPath, this)) {
            qDebug() << "could not register" << counterPath;
        } else {
            setRunning(shouldBeRunning);
        }
    }
}

/*
 *This is the dbus adaptor to the connman interface
 **/
CounterAdaptor::CounterAdaptor(Counter* parent)
  : QDBusAbstractAdaptor(parent),
    m_counter(parent)
{
}

CounterAdaptor::~CounterAdaptor()
{
}

void CounterAdaptor::Release()
{
     m_counter->release();
}

void CounterAdaptor::Usage(const QDBusObjectPath &service_path,
                           const QVariantMap &home,
                           const QVariantMap &roaming)
{
    if (!home.isEmpty()) {
        // home
        m_counter->serviceUsage(service_path.path(), home, false);
    }
    if (!roaming.isEmpty()) {
        //roaming
        m_counter->serviceUsage(service_path.path(), roaming, true);
    }
}

