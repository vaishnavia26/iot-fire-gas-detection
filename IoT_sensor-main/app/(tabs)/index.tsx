import React from 'react';
import { SafeAreaView, ScrollView, View } from 'react-native';


import ESP32SensorMonitor from '../../components/ESP32SensorMonitor';

export default function TabOneScreen() {
  return (
    <SafeAreaView>
      <ScrollView>
        <View style={{ paddingVertical: 150, padding: 10}}>
          <ESP32SensorMonitor />
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}