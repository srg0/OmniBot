import React, { useState } from 'react';

const SetupDevice = ({ state, setters, actions }) => {
  const { bleDevices, bleScanMessage, isScanning, selectedDevice } = state;
  const { setSetupStep, setSelectedDevice } = setters;
  const { handleScan, handleWifiScan } = actions;

  return (
    <div className="setup-section fade-in">
      <button 
        className={`btn btn-primary btn-block ${isScanning ? 'scanning' : ''}`}
        onClick={handleScan}
        disabled={isScanning}
      >
        {isScanning ? (
          <><span className="spinner"></span> Scanning for Devices...</>
        ) : (
          'Scan for Nearby Bots'
        )}
      </button>

      {bleScanMessage && (
        <p className="help-text" style={{ marginBottom: '1rem' }} role="status">
          {bleScanMessage}
        </p>
      )}

      <div className="device-list">
        {bleDevices.map(d => (
          <div 
            key={d.address} 
            className={`device-item ${selectedDevice?.address === d.address ? 'selected' : ''}`}
            onClick={() => {
              setSelectedDevice(d);
              setSetupStep('wifi');
              handleWifiScan();
            }}
          >
            <div className="device-icon">🤖</div>
            <div className="device-info">
              <span className="device-name">{d.name}</span>
              <span className="device-mac">{d.address}</span>
            </div>
            <div className="device-arrow">→</div>
          </div>
        ))}
        {bleDevices.length === 0 && !isScanning && !bleScanMessage && (
          <div className="empty-state">No nearby devices found. Make sure the bot is powered on.</div>
        )}
      </div>
    </div>
  );
};

export default SetupDevice;
