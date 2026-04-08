import React, { useEffect, useMemo, useRef, useState } from 'react';
import './IntelligenceFeed.css';

const SearchGroundingBlock = ({ sources, queries }) => {
  const hasSources = sources && sources.length > 0;
  const hasQueries = queries && queries.length > 0;
  if (!hasSources && !hasQueries) return null;
  return (
    <div className="maps-grounding-block">
      {hasSources && (
        <div className="maps-grounding-sources">
          <p className="maps-grounding-line">
            {sources.map((s, i) => (
              <span key={s.uri}>
                {i > 0 && <span>, </span>}
                <a href={s.uri} target="_blank" rel="noopener noreferrer">
                  {s.title || 'Web source'}
                </a>
              </span>
            ))}
          </p>
          <p className="gmp-attribution" translate="no">
            Google Search
          </p>
        </div>
      )}
      {hasQueries && (
        <p className="maps-widget-hint">
          Search queries: {queries.join(' | ')}
        </p>
      )}
    </div>
  );
};

function formatToolArguments(args) {
  try {
    return JSON.stringify(args, null, 2);
  } catch {
    return String(args);
  }
}

const IntelligenceFeed = ({
  logs,
  toolCalls = [],
  hubActivityLog = [],
  selectedBotId = 'default_bot',
  wsStatus,
  textMessage,
  setTextMessage,
  isSendingText,
  onSendTextCommand,
  onGiveSoul,
}) => {
  const logEndRef = useRef(null);
  const textInputRef = useRef(null);
  const [hubLogOpen, setHubLogOpen] = useState(false);

  const chatLogs = useMemo(
    () =>
      logs.filter(
        (log) => log.sender !== 'tool' && log.sender !== 'system' && log.sender !== 'error'
      ),
    [logs]
  );

  const toolCallsForBot = useMemo(
    () => toolCalls.filter((t) => (t.deviceId || 'default_bot') === selectedBotId),
    [toolCalls, selectedBotId]
  );

  const hubLogCount = toolCallsForBot.length + hubActivityLog.length;

  useEffect(() => {
    logEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [chatLogs]);

  useEffect(() => {
    if (!isSendingText) {
      textInputRef.current?.focus();
    }
  }, [isSendingText]);

  return (
    <div className="intelligence-feed">
      <header className="feed-header">
        <h2>Intelligence Feed</h2>
        <div className="feed-header-actions">
          <button
            type="button"
            className={`feed-tool-log-btn ${hubLogOpen ? 'active' : ''}`}
            onClick={() => setHubLogOpen((o) => !o)}
            title="Tool calls, hub system messages, and errors"
          >
            Hub log
            {hubLogCount > 0 && <span className="feed-tool-log-badge">{hubLogCount}</span>}
          </button>
          {onGiveSoul && (
            <button
              type="button"
              className="feed-soul-btn"
              onClick={onGiveSoul}
              disabled={isSendingText}
              title="Reset chat history and start the first-run soul ritual (BOOTSTRAP.md)"
            >
              Give me a soul
            </button>
          )}
          <div className={`ws-badge ${wsStatus}`}>
            <div className="status-dot"></div>
            {wsStatus === 'connected' ? 'Core Connected' : 'Core Disconnected'}
          </div>
        </div>
      </header>

      {hubLogOpen && (
        <div className="tool-log-panel" aria-label="Hub activity log">
          <div className="tool-log-panel-head">
            <span>Hub activity</span>
            <span className="tool-log-hint">Newest at the bottom</span>
          </div>
          <div className="tool-log-scroll">
            <h3 className="tool-log-section-title">Tool calls — {selectedBotId}</h3>
            {toolCallsForBot.length === 0 ? (
              <p className="tool-log-empty">No tool calls for this bot yet.</p>
            ) : (
              toolCallsForBot.map((tc) => (
                <div key={tc.id} className="tool-log-entry">
                  <div className="tool-log-entry-top">
                    <time className="tool-log-time">{tc.time}</time>
                    <code className="tool-log-fn">{tc.functionName}</code>
                  </div>
                  <details className="tool-log-details">
                    <summary>Arguments</summary>
                    <pre className="tool-log-args">{formatToolArguments(tc.arguments)}</pre>
                  </details>
                </div>
              ))
            )}

            <h3 className="tool-log-section-title">System and errors</h3>
            {hubActivityLog.length === 0 ? (
              <p className="tool-log-empty">No system messages or errors yet.</p>
            ) : (
              hubActivityLog.map((ev) => (
                <div
                  key={ev.id}
                  className={`hub-activity-entry hub-activity-entry--${ev.sender}`}
                >
                  <div className="hub-activity-meta">
                    <time className="tool-log-time">{ev.time}</time>
                    <span className="hub-activity-label">
                      {ev.sender === 'error' ? 'Error' : 'System'}
                    </span>
                  </div>
                  <p className="hub-activity-text">{ev.text}</p>
                </div>
              ))
            )}
          </div>
        </div>
      )}

      <div className="feed-content">
        {chatLogs.length === 0 ? (
          <div className="empty-feed">
            <div className="pulse-ring"></div>
            <p>Waiting for sensory data...</p>
          </div>
        ) : (
          <div className="messages-area">
            {chatLogs.map((log) => {
              return (
                <div key={log.id} className={`message-bubble ${log.sender}`}>
                  <div className="message-meta">
                    <span className="message-sender">
                      {log.sender === 'esp32' && 'Pixel Bot'}
                      {log.sender === 'video' && 'Pixel Bot (Video)'}
                      {log.sender === 'audio' && 'Pixel Bot (Audio)'}
                      {log.sender === 'ai' && 'Gemini AI'}
                      {log.sender === 'user' && 'You'}
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
                      <audio className="message-audio" src={log.text} controls />
                    ) : (
                      <>
                        {log.text}
                        {log.sender === 'ai' && (
                          <>
                            <SearchGroundingBlock
                              sources={log.searchSources}
                              queries={log.searchQueries}
                            />
                          </>
                        )}
                      </>
                    )}
                  </div>
                </div>
              );
            })}
            <div ref={logEndRef} />
          </div>
        )}
      </div>

      <form className="text-command-bar" onSubmit={onSendTextCommand}>
        <input
          ref={textInputRef}
          type="text"
          className="text-command-input"
          placeholder="Type a message to Pixel..."
          value={textMessage}
          onChange={(e) => setTextMessage(e.target.value)}
        />
        <button
          type="submit"
          className="text-command-send"
          disabled={isSendingText || !textMessage.trim()}
        >
          {isSendingText ? 'Sending...' : 'Send'}
        </button>
      </form>
    </div>
  );
};

export default IntelligenceFeed;
