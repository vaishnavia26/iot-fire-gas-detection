import { AWSIoTProvider } from '@aws-amplify/pubsub';
import { Amplify, PubSub } from 'aws-amplify';
import React, { useCallback, useEffect, useRef, useState } from 'react';
import { ActivityIndicator, Alert, Button, Text, TextInput, View } from 'react-native';

import styles from './ESP32SensorMonitor.styles';

interface SensorData {
  temperature: number | null;
  gasValue: number | null;
  flameAlert: boolean | null;
  gasDetected: boolean | null;
  highTemperature: boolean | null;
}

const ESP32SensorMonitor: React.FC = () => {
  // Provisioning (ESP AP) state
  const [espIp, setEspIp] = useState('');
  const [espSsid, setEspSsid] = useState('');
  const [espPassword, setEspPassword] = useState('');
  const [tempThre, setTempThre] = useState('');
  const handleTempThreChange = (input) => {
    const numericValue = input.replace(/[^0-9]/g, '');
    setTempThre(numericValue);
  };
  const [isFlashing, setIsFlashing] = useState(false);
  const [flashed, setFlashed] = useState(false);

  // WiFi input state
  const [appWifiSsid, setAppWifiSsid] = useState('');
  const [appPassword, setAppPassword] = useState('');
  const [wifiConnected, setWifiConnected] = useState(false);
  // AWS IoT sensor data endpoint and state
  const [awsEndpoint, setAwsEndpoint] = useState('');
  const [serialID, setSerialID] = useState('');
  const [dataLoaded, setDataLoaded] = useState(false);
  const [sensorData, setSensorData] = useState<SensorData>({
    temperature: null,
    gasValue: null,
    flameAlert: null,
    gasDetected: null,
    highTemperature: null,
  });
  const [awsError, setAwsError] = useState(null);
  const [alertStatus, setAlertStatus] = useState(false);
  const [lastFetch, setLastFetch] = useState<string>('');

  const ws = useRef<WebSocket | null>(null);
  // Step 1: send credentials to ESP AP
  const connectWebSocket = useCallback(() => {    
    if (!espIp || !espSsid || !espPassword || !tempThre) {
      Alert.alert('Please enter connection IP, SSID, password and sensor alert.');
      return;
    }    
    if (tempThre < 0 || tempThre > 100) {
      Alert.alert('Error', 'Temperature threshold must be between 0 and 100.');
      return;
    }
    setIsFlashing(true);

    ws.current = new WebSocket(`ws://${espIp}:81`);
    ws.current.onopen = () => {
      ws.current?.send(JSON.stringify({ ssid: espSsid, password: espPassword, tempThre: tempThre }));
    };
    ws.current.onclose = () => {
      setIsFlashing(false);
      setFlashed(true);
      Alert.alert('Please check the device OLED to confirm set up is successfully');
      ws.current = null;
    };
    ws.current.onerror = (err) => {
      console.error('WebSocket error:', err);
      setIsFlashing(false);
      setFlashed(false);
      ws.current = null;
    };
  }, [espIp, espSsid, espPassword, tempThre]);

  // Setp 2. connect to WiFi and AWS IoT endpoint
  const connectWifi = useCallback(() => {
    if (!appWifiSsid || !appPassword || !awsEndpoint || !serialID) {
      Alert.alert('Error', 'Please enter all the fields.');
      return;
    }
    const endpoint = 'wss://' + awsEndpoint + 'ytefhcl-ats.iot.us-east-1.amazonaws.com/mqtt';
    console.log('Endpoint:', endpoint);
    Amplify.configure({
      Auth: {
        identityPoolId: 'us-east-1:28dc8ade-f3b2-41f9-b6d5-82254c3093c8',
        region: 'us-east-1',
      },
    });
    // Register the IoT “pluggable”
    Amplify.addPluggable(new AWSIoTProvider({
      aws_pubsub_region: 'us-east-1',
      aws_pubsub_endpoint: endpoint
    }));

    setWifiConnected(true);
  }, [appWifiSsid, appPassword, awsEndpoint, serialID]);

  // get IoT reading from AWS once wifiConnected
  useEffect(() => {
    if (!wifiConnected) return;
    const subscription = PubSub.subscribe(serialID).subscribe({
      next: (data) => {

        const timestamp = new Date().toLocaleString();
        console.log('MQTT msg:', timestamp, data);
        setLastFetch(timestamp);

        const payload = typeof data.value === 'string' ? JSON.parse(data.value) : data.value;
        setSensorData({
          temperature: payload.temperature,
          gasValue: payload.gasValue,
          flameAlert: payload.flameAlert,
          gasDetected: payload.gasAlert,
          highTemperature: payload.tempAlert,
        });
        setAlertStatus(
          payload.gasValue >= 400 ||
          payload.flameAlert == true ||
          payload.gasAlert == true ||
          payload.tempAlert == true
        );
        setDataLoaded(true);     
        if (alertStatus) {
          Alert.alert('Alert', 'Risk detected');
        }

      },
      error: (err) => {
        console.error(err);
        setAwsError(err);
      },
    });
    return () => subscription.unsubscribe();
  }, [wifiConnected, dataLoaded, awsError, alertStatus]);

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Home Fire Safety Monitor</Text>

      {!flashed && (
        <>
          <Text style={styles.statusText}>Step 1: Set up sensor and internet connection</Text>
          <TextInput
            style={styles.input}
            placeholder="IP address shows on the device"
            value={espIp}
            onChangeText={setEspIp}
          />
          <TextInput
            style={styles.input}
            placeholder="Wi-Fi SSID for the device"
            value={espSsid}
            onChangeText={setEspSsid}
          />
          <TextInput
            style={styles.input}
            placeholder="Wi-Fi password for the device"
            secureTextEntry
            value={espPassword}
            onChangeText={setEspPassword}
          />
          <TextInput
            style={styles.input}
            placeholder="Temmperature threshold for buzzer"
            value={tempThre}
            keyboardType='numeric'
            onChangeText={handleTempThreChange}
          />          
          
          <View style={styles.button}>
            <Button
              title={isFlashing ? 'Configuring ...' : 'Set up'}
              onPress={connectWebSocket}
              disabled={isFlashing}
            />
          </View>
        </>
      )}

      {flashed && !wifiConnected && (
        <>
          <Text style={styles.statusText}>Step 2: Connet App to WiFi</Text>
          <Text style={styles.subText}>Connet App to WiFi</Text>
          <TextInput
            style={styles.input}
            placeholder="SSID"
            value={appWifiSsid}
            onChangeText={setAppWifiSsid}
          />
          <TextInput
            style={styles.input}
            placeholder="Password"
            secureTextEntry
            value={appPassword}
            onChangeText={setAppPassword}
          />
          <Text style={styles.subText}>Link up your device, enter the following shows on the device</Text>
          <TextInput
            style={styles.input}
            placeholder="SerialID"
            value={awsEndpoint}
            onChangeText={setAwsEndpoint}
          />
          <TextInput
            style={styles.input}
            placeholder="DeviceID"
            value={serialID}
            onChangeText={setSerialID}
          />

          <View style={styles.button}>
            <Button
              title="Connect to Wi-Fi"
              onPress={connectWifi}
            />
          </View>
        </>
      )}

      {/* connecting stage */}
      {wifiConnected && !dataLoaded && (
        <View style={styles.loadingContainer}>
          <ActivityIndicator size="large" />
          <Text style={styles.loadingText}>Loading sensor data</Text>
        </View>
      )}
      {awsError && (
        <Text style={styles.errorText}>Connection fails. Please try again. </Text>
      )} 
      
      {/* Step 3: subscribe data */}
      {wifiConnected && dataLoaded && !awsError && (
        <View style={styles.dataContainer}>
          <View style={styles.timestampContainer}>         
            <Text style={styles.timestampText}>Last update: {lastFetch}</Text>
          </View>

          <View style={styles.dataRow}>
            <View style={styles.dataItem}>
              <Text style={styles.dataLabel}>Temperature:</Text>
              <Text style={styles.dataText}>{sensorData.temperature ?? '–––'} °C</Text>
            </View>
            <View style={styles.dataItem}>
              <Text style={styles.dataLabel}>Gas level:</Text>
              <Text style={styles.dataText}>{sensorData.gasValue ?? '–––'}</Text>
            </View>
          </View>

          <View style={styles.alertsContainer}>
            {sensorData.gasDetected && (
              <View style={styles.alertItem}>
                <Text style={styles.alertText}>Gas Leak !!</Text>
              </View>
            )}
            {sensorData.highTemperature && (
              <View style={styles.alertItem}>
                <Text style={styles.alertText}>High Temp !!</Text>
              </View>
            )}
            {sensorData.flameAlert && (
              <View style={styles.alertItem}>
                <Text style={styles.alertText}>Flame !!</Text>
              </View>
            )}
          </View>
          
          {alertStatus && (
            <View style={styles.alertIndicatorContainer}>
              <View style={styles.redCircle} />              
            </View>
          )}
          {!alertStatus && (
            <View style={styles.alertIndicatorContainer}>
              <View style={styles.greenCircle} />              
            </View>
          )}
        </View>
      )}
    </View>
  );
};

export default ESP32SensorMonitor;