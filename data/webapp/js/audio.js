/* =================================================================
   Audio Feedback System - "Geiger Counter for LoRa"
   Different sounds for different protocols and events
   ================================================================= */

class AudioFeedback {
    constructor() {
        this.audioContext = null;
        this.enabled = true;
        this.volume = 0.3; // Default volume (0.0 - 1.0)
        this.lastPacketTime = 0;
        this.packetCount = 0;
        
        // Initialize audio context on first user interaction
        this.initAudioContext();
    }
    
    initAudioContext() {
        // Create audio context on first interaction (browser requirement)
        document.addEventListener('click', () => {
            if (!this.audioContext) {
                this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
                console.log('[Audio] Context initialized');
            }
        }, { once: true });
    }
    
    // Beep for packet capture - different tones for different protocols
    playPacketCapture(protocol = 'Unknown') {
        if (!this.enabled || !this.audioContext) return;
        
        // Protocol-specific frequencies (Hz)
        const tones = {
            'Meshtastic': 800,  // Higher pitched
            'LoRaWAN': 600,     // Medium pitch
            'Helium': 500,      // Lower pitched
            'Unknown': 400      // Lowest pitch
        };
        
        // Find matching protocol
        let frequency = tones.Unknown;
        for (const [key, value] of Object.entries(tones)) {
            if (protocol.includes(key)) {
                frequency = value;
                break;
            }
        }
        
        this.playBeep(frequency, 0.08, 'sine'); // Short beep
        this.packetCount++;
    }
    
    // Click for device discovery
    playDeviceDiscovered() {
        if (!this.enabled || !this.audioContext) return;
        this.playBeep(1000, 0.15, 'square'); // Distinctive click sound
    }
    
    // Success tone for capture complete
    playCaptureComplete() {
        if (!this.enabled || !this.audioContext) return;
        // Two-tone success sound
        this.playBeep(600, 0.1, 'sine');
        setTimeout(() => this.playBeep(800, 0.15, 'sine'), 100);
    }
    
    // Warning tone for mode changes
    playModeChange() {
        if (!this.enabled || !this.audioContext) return;
        this.playBeep(500, 0.2, 'triangle');
    }
    
    // Rapid clicks when lots of activity (Geiger counter effect)
    playActivityBurst() {
        if (!this.enabled || !this.audioContext) return;
        const now = Date.now();
        const timeSinceLastPacket = now - this.lastPacketTime;
        this.lastPacketTime = now;
        
        // Increase pitch/speed as packet rate increases
        if (timeSinceLastPacket < 500) {
            this.playBeep(900, 0.05, 'sine');
        } else {
            this.playBeep(700, 0.08, 'sine');
        }
    }
    
    // Core beep function using Web Audio API
    playBeep(frequency, duration, type = 'sine') {
        if (!this.audioContext) return;
        
        try {
            const oscillator = this.audioContext.createOscillator();
            const gainNode = this.audioContext.createGain();
            
            oscillator.connect(gainNode);
            gainNode.connect(this.audioContext.destination);
            
            oscillator.type = type; // 'sine', 'square', 'triangle', 'sawtooth'
            oscillator.frequency.value = frequency;
            
            // Envelope: quick attack, exponential decay
            const now = this.audioContext.currentTime;
            gainNode.gain.setValueAtTime(0, now);
            gainNode.gain.linearRampToValueAtTime(this.volume, now + 0.01); // Attack
            gainNode.gain.exponentialRampToValueAtTime(0.01, now + duration); // Decay
            
            oscillator.start(now);
            oscillator.stop(now + duration);
        } catch (error) {
            console.warn('[Audio] Playback error:', error);
        }
    }
    
    // Toggle audio on/off
    toggle() {
        this.enabled = !this.enabled;
        return this.enabled;
    }
    
    // Set volume (0.0 - 1.0)
    setVolume(vol) {
        this.volume = Math.max(0, Math.min(1, vol));
    }
    
    // Get current state
    isEnabled() {
        return this.enabled;
    }
}

// Global instance
window.audioFeedback = new AudioFeedback();
