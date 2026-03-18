import React from 'react';
import './Sidebar.css';

const Sidebar = ({ 
  esp32Status, 
  lastPing, 
  appMode, 
  setAppMode, 
  setSetupStep 
}) => {
  return (
    <aside className="sidebar">
      <div className="sidebar-brand">
        <h1>Prism</h1>
      </div>

      <div className="sidebar-section">
        <h3>Connected Bots</h3>
        
        {/* Pixel Bot Status Card */}
        <div className={`bot-card ${esp32Status}`}>
          <div className="bot-card-header">
            <div className="bot-icon">
              <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" className="holo-icon">
                <path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/>
              </svg>
            </div>
            <div className="bot-info">
              <span className="bot-name">Pixel</span>
              <span className="bot-status-text">
                {esp32Status === 'offline' && 'Offline'}
                {esp32Status === 'online' && 'Ready'}
                {esp32Status === 'working' && 'Processing'}
              </span>
            </div>
          </div>
          {lastPing && esp32Status !== 'offline' && (
            <div className="bot-ping">Last seen: {lastPing}</div>
          )}
        </div>
      </div>

      <div className="sidebar-actions">
        <button 
          className={`nav-btn ${appMode === 'dashboard' ? 'active' : ''}`}
          onClick={() => setAppMode('dashboard')}
        >
          <span className="nav-icon">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <polyline points="22 12 18 12 15 21 9 3 6 12 2 12"></polyline>
            </svg>
          </span>
          Intelligence Feed
        </button>
        
        <button 
          className={`nav-btn ${appMode === 'setup' ? 'active' : ''}`}
          onClick={() => {
            setAppMode('setup');
            setSetupStep('device');
          }}
        >
          <span className="nav-icon">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <circle cx="12" cy="12" r="10"></circle>
              <line x1="12" y1="8" x2="12" y2="16"></line>
              <line x1="8" y1="12" x2="16" y2="12"></line>
            </svg>
          </span>
          Add New Bot
        </button>
      </div>
    </aside>
  );
};

export default Sidebar;
