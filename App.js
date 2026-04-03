import React, { useState, useEffect } from 'react';
import { StyleSheet, Text, View, TouchableOpacity, ScrollView, Animated, StatusBar } from 'react-native';
import { Shield, Settings, Zap, Cpu, Activity, AlertCircle, CheckCircle2 } from 'lucide-react-native';

const COLORS = {
  background: '#0a0b10',
  surface: '#15161c',
  primary: '#e94560', // Blood Red
  secondary: '#00ff00', // Neon Green
  text: '#ffffff',
  textSecondary: '#a0a0a0',
  accent: '#1e212d',
  danger: '#ff4b5c',
  success: '#4caf50'
};

export default function App() {
  const [selectedMethod, setSelectedMethod] = useState('ptrace');
  const [status, setStatus] = useState('idle'); // idle, injecting, success, error
  const [fadeAnim] = useState(new Animated.Value(0));
  const [systemStats, setSystemStats] = useState({
    gameRunning: false,
    ptraceScope: '...',
    seLinux: '...',
    logs: []
  });

  useEffect(() => {
    Animated.timing(fadeAnim, {
      toValue: 1,
      duration: 1000,
      useNativeDriver: true,
    }).start();
  }, []);

  // 🩺 System Doctor Polling
  useEffect(() => {
    const pollStatus = async () => {
      try {
        const response = await fetch('http://localhost:3001/status');
        const data = await response.json();
        setSystemStats(prev => ({ ...prev, ...data }));
      } catch (err) {
        console.log("Bridge not found");
      }
    };

    const interval = setInterval(pollStatus, 2000);
    pollStatus();
    return () => clearInterval(interval);
  }, []);

  const handleInject = async () => {
    setStatus('injecting');
    setSystemStats(prev => ({ ...prev, logs: ["[info] Initializing injection sequence...", ...prev.logs] }));
    
    try {
      const response = await fetch('http://localhost:3001/inject', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ method: selectedMethod, pid: systemStats.pid })
      });
      const data = await response.json();
      
      if (data.success) {
        setStatus('success');
        setSystemStats(prev => ({ 
          ...prev, 
          logs: [data.output.split('\n').filter(l => l).reverse()[0], ...prev.logs] 
        }));
      } else {
        setStatus('error');
        setSystemStats(prev => ({ ...prev, logs: ["[error] Injection failed", ...prev.logs] }));
      }
    } catch (err) {
      setStatus('error');
    }
  };

  return (
    <View style={styles.container}>
      <StatusBar barStyle="light-content" />
      <Animated.ScrollView style={[styles.scrollView, { opacity: fadeAnim }]}>
        {/* Header */}
        <View style={styles.header}>
          <Text style={styles.title}>BLOODSTRIKE <Text style={styles.highlight}>LINUX</Text></Text>
          <Text style={styles.subtitle}>ADVANCED STEALTH INJECTOR</Text>
        </View>

        {/* System Stats Row */}
        <View style={styles.statsRow}>
          <StatBox icon={<Cpu size={20} color={COLORS.primary} />} label="CPU" value="Low" />
          <StatBox icon={<Activity size={20} color={COLORS.secondary} />} label="STATE" value={systemStats.gameRunning ? "RUNNING" : "Ready"} />
          <StatBox icon={<Shield size={20} color={COLORS.primary} />} label="VAC" value="SAFE" />
        </View>

        {/* System Doctor */}
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>SYSTEM DOCTOR</Text>
          <View style={styles.card}>
            <StatusItem 
              label="Ptrace Scope (yama)" 
              status={systemStats.ptraceScope} 
              icon={<CheckCircle2 size={18} color={systemStats.ptraceScope === 'OK' ? COLORS.success : COLORS.danger} />} 
            />
            <StatusItem 
              label="SELinux Enforcement" 
              status={systemStats.seLinux} 
              icon={<CheckCircle2 size={18} color={COLORS.success} />} 
            />
            <StatusItem 
              label="Target Process (BloodStrike)" 
              status={systemStats.gameRunning ? "FOUND" : "NOT FOUND"} 
              icon={systemStats.gameRunning ? 
                <CheckCircle2 size={18} color={COLORS.success} /> : 
                <AlertCircle size={18} color={COLORS.danger} />
              } 
            />
          </View>
        </View>

        {/* Injection Configuration */}
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>INJECTION CONFIG</Text>
          <View style={styles.card}>
            <Text style={styles.cardLabel}>Select Method</Text>
            <View style={styles.methodsRow}>
              {['ptrace', 'memfd', 'proton'].map((method) => (
                <TouchableOpacity 
                  key={method}
                  style={[styles.methodButton, selectedMethod === method && styles.activeMethod]}
                  onPress={() => setSelectedMethod(method)}
                >
                  <Text style={[styles.methodText, selectedMethod === method && styles.activeMethodText]}>
                    {method.toUpperCase()}
                  </Text>
                </TouchableOpacity>
              ))}
            </View>

            <TouchableOpacity 
              style={[styles.injectButton, status === 'injecting' && styles.injectButtonDisabled]}
              onPress={handleInject}
              disabled={status === 'injecting'}
            >
              <Zap size={20} color="#fff" />
              <Text style={styles.injectButtonText}>
                {status === 'injecting' ? 'INJECTING...' : 'INJECT CHEAT'}
              </Text>
            </TouchableOpacity>

            {status === 'success' && (
              <Text style={styles.statusSuccess}>Injection Successful! Press INSERT in-game.</Text>
            )}
          </View>
        </View>

        {/* Logs Preview */}
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>CONSOLE OUTPUT</Text>
          <View style={styles.logBox}>
            {systemStats.logs.length > 0 ? systemStats.logs.map((log, i) => (
              <Text key={i} style={styles.logText}>{log}</Text>
            )) : (
              <>
                <Text style={styles.logText}>[info] Searching for BloodStrike.exe...</Text>
                <Text style={styles.logText}>[ptrace] Initializing attachment sequence...</Text>
                <Text style={styles.logText}>[symbol] Found libc at 0x7FFFF7D9F000</Text>
                <Text style={styles.logText}>[success] Memory allocated successfully</Text>
              </>
            )}
          </View>
        </View>
      </Animated.ScrollView>
    </View>
  );
}

const StatBox = ({ icon, label, value }) => (
  <View style={styles.statBox}>
    {icon}
    <View style={{ marginLeft: 10 }}>
      <Text style={styles.statLabel}>{label}</Text>
      <Text style={styles.statValue}>{value}</Text>
    </View>
  </View>
);

const StatusItem = ({ label, status, icon }) => (
  <View style={styles.statusItem}>
    <Text style={styles.statusLabel}>{label}</Text>
    <View style={styles.statusValueContainer}>
      <Text style={[styles.statusValue, status === 'NOT FOUND' && { color: COLORS.danger }]}>{status}</Text>
      {icon}
    </View>
  </View>
);

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
    paddingTop: 40,
  },
  scrollView: {
    padding: 20,
  },
  header: {
    marginBottom: 30,
    alignItems: 'center',
  },
  title: {
    fontSize: 28,
    fontWeight: '900',
    color: '#fff',
    letterSpacing: 2,
  },
  highlight: {
    color: COLORS.primary,
  },
  subtitle: {
    fontSize: 10,
    color: COLORS.textSecondary,
    letterSpacing: 4,
    marginTop: 5,
  },
  statsRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 20,
  },
  statBox: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: COLORS.surface,
    padding: 12,
    borderRadius: 12,
    flex: 0.32,
    borderWidth: 1,
    borderColor: 'rgba(233, 69, 96, 0.1)',
  },
  statLabel: {
    color: COLORS.textSecondary,
    fontSize: 9,
    fontWeight: 'bold',
  },
  statValue: {
    color: '#fff',
    fontSize: 13,
    fontWeight: 'bold',
  },
  section: {
    marginBottom: 25,
  },
  sectionTitle: {
    color: COLORS.primary,
    fontSize: 12,
    fontWeight: 'bold',
    letterSpacing: 1.5,
    marginBottom: 10,
    marginLeft: 5,
  },
  card: {
    backgroundColor: COLORS.surface,
    borderRadius: 16,
    padding: 15,
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.05)',
  },
  cardLabel: {
    color: COLORS.textSecondary,
    fontSize: 12,
    marginBottom: 10,
  },
  statusItem: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 10,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(255, 255, 255, 0.05)',
  },
  statusLabel: {
    color: COLORS.text,
    fontSize: 14,
  },
  statusValueContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  statusValue: {
    fontSize: 12,
    color: COLORS.success,
    fontWeight: '800',
  },
  methodsRow: {
    flexDirection: 'row',
    gap: 10,
    marginBottom: 20,
  },
  methodButton: {
    flex: 1,
    backgroundColor: COLORS.accent,
    padding: 12,
    borderRadius: 10,
    alignItems: 'center',
    borderWidth: 1,
    borderColor: 'transparent',
  },
  activeMethod: {
    borderColor: COLORS.primary,
    backgroundColor: 'rgba(233, 69, 96, 0.1)',
  },
  methodText: {
    color: COLORS.textSecondary,
    fontSize: 11,
    fontWeight: 'bold',
  },
  activeMethodText: {
    color: COLORS.primary,
  },
  injectButton: {
    backgroundColor: COLORS.primary,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    padding: 16,
    borderRadius: 12,
    gap: 10,
    shadowColor: COLORS.primary,
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 8,
    elevation: 8,
  },
  injectButtonDisabled: {
    opacity: 0.5,
  },
  injectButtonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
    letterSpacing: 1,
  },
  statusSuccess: {
    color: COLORS.success,
    textAlign: 'center',
    marginTop: 15,
    fontSize: 12,
  },
  logBox: {
    backgroundColor: '#050505',
    borderRadius: 12,
    padding: 15,
    minHeight: 100,
    borderWidth: 1,
    borderColor: 'rgba(233, 69, 96, 0.2)',
  },
  logText: {
    color: '#00ff00',
    fontFamily: 'monospace',
    fontSize: 11,
    lineHeight: 18,
  }
});
