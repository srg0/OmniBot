import React, { useEffect, useRef } from 'react';
import './IntelligenceFeed.css';

const IntelligenceFeed = ({ logs, wsStatus }) => {
  const logEndRef = useRef(null);

  useEffect(() => {
    logEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [logs]);

  return (
    <div className="intelligence-feed">
      <header className="feed-header">
        <h2>Intelligence Feed</h2>
        <div className={`ws-badge ${wsStatus}`}>
          <div className="status-dot"></div>
          {wsStatus === 'connected' ? 'Core Connected' : 'Core Disconnected'}
        </div>
      </header>
      
      <div className="feed-content">
        {logs.length === 0 ? (
          <div className="empty-feed">
            <div className="pulse-ring"></div>
            <p>Waiting for sensory data...</p>
          </div>
        ) : (
          <div className="messages-area">
            {logs.map((log) => (
              <div key={log.id} className={`message-bubble ${log.sender}`}>
                <div className="message-meta">
                  <span className="message-sender">
                    {log.sender === 'system' && 'OmniBot Core'}
                    {log.sender === 'esp32' && 'Pixel Bot'}
                    {log.sender === 'video' && 'Pixel Bot (Video)'}
                    {log.sender === 'audio' && 'Pixel Bot (Audio)'}
                    {log.sender === 'ai' && 'Gemini AI'}
                    {log.sender === 'error' && 'System Error'}
                  </span>
                  <span className="message-time">{log.time}</span>
                </div>
                <div className="message-text">
                  {log.sender === 'video' ? (
                    <video
                      className="message-video"
                      src={log.text}
                      controls
                      autoPlay
                      loop
                      muted
                      playsInline
                    />
                  ) : log.sender === 'audio' ? (
                    <audio
                      className="message-audio"
                      src={log.text}
                      controls
                    />
                  ) : (
                    log.text
                  )}
                </div>
              </div>
            ))}
            <div ref={logEndRef} />
          </div>
        )}
      </div>
    </div>
  );
};

export default IntelligenceFeed;
