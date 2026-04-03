import React from 'react';

const SetupWifi = ({ state, setters, actions }) => {
  const { wifiNetworks, wifiScanMessage, isScanningWifi } = state;
  const { setSsid, setPassword, setSetupStep } = setters;
  const { handleWifiScan } = actions;

  return (
    <div className="setup-section slide-enter">
      <h3 className="section-title">Select Wi‑Fi network</h3>
      <p className="help-text" style={{ marginBottom: '1rem' }}>
        Choose the network Pixel should join. Use refresh below if the list is incomplete.
      </p>
      {wifiScanMessage && (
        <p className="help-text" style={{ marginBottom: '1rem' }}>
          {wifiScanMessage}
        </p>
      )}
      {isScanningWifi ? (
        <div className="scanning-container">
          <div className="pulse-ring mx-auto"></div>
          <p>Scanning Wi‑Fi…</p>
        </div>
      ) : (
        <div className="device-list wifi-list">
          {wifiNetworks.map((net, i) => (
            <div
              key={i}
              className="device-item"
              onClick={() => {
                setSsid(net);
                setPassword('');
                setSetupStep('password');
              }}
            >
              <div className="device-icon wifi-icon">📶</div>
              <div className="device-info">
                <span className="device-name">{net}</span>
              </div>
            </div>
          ))}
          {wifiNetworks.length === 0 && <div className="empty-state">No networks found.</div>}

          <div
            className="device-item manual-network"
            onClick={() => {
              setSsid('');
              setPassword('');
              setSetupStep('password');
            }}
          >
            <div className="device-icon">➕</div>
            <div className="device-info">
              <span className="device-name">Join Other Network…</span>
            </div>
          </div>
        </div>
      )}

      <button
        type="button"
        className="btn btn-secondary btn-block mt-3"
        onClick={() => handleWifiScan()}
        disabled={isScanningWifi}
      >
        {isScanningWifi ? 'Scanning…' : 'Refresh Wi‑Fi list'}
      </button>

      <button className="btn btn-secondary btn-block mt-4" onClick={() => setSetupStep('device')}>
        Back to bot selection
      </button>
    </div>
  );
};

export default SetupWifi;
