import React from 'react';
import './SetupFlow.css';
import SetupDevice from './SetupFlow/SetupDevice';
import SetupWifi from './SetupFlow/SetupWifi';
import SetupPassword from './SetupFlow/SetupPassword';

const SetupOrchestrator = ({ 
  state, 
  setters, 
  actions 
}) => {
  const { setupStep } = state;
  const { setAppMode } = setters;

  return (
    <div className="setup-container">
      <div className="setup-card">
        <div className="setup-header">
          <h1>Add New Bot</h1>
          <p className="setup-subtitle">Connect your hardware to the OmniBot Hub.</p>
        </div>

        <div className="setup-body">
          {setupStep === 'device' && (
            <SetupDevice 
              state={state} 
              setters={setters} 
              actions={actions} 
            />
          )}

          {setupStep === 'wifi' && (
            <SetupWifi 
              state={state} 
              setters={setters} 
              actions={actions} 
            />
          )}

          {setupStep === 'password' && (
            <SetupPassword 
              state={state} 
              setters={setters} 
              actions={actions} 
            />
          )}
        </div>

        <div className="setup-footer">
          <button className="text-btn" onClick={() => setAppMode('dashboard')}>
            Cancel & Return
          </button>
        </div>
      </div>
    </div>
  );
};

export default SetupOrchestrator;
