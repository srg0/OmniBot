import { useState } from 'react';
import { postHubSettings } from './setupService';
import './FirstRunSetup.css';

const AI_STUDIO_URL = 'https://aistudio.google.com/apikey';

/**
 * Shown when the hub is up but no Gemini API key is configured yet.
 * Saves the key via POST /api/hub/settings → hub_secrets.json (no manual files).
 */
export default function FirstRunSetup({ onConfigured }) {
  const [key, setKey] = useState('');
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(null);

  const handleSubmit = async (e) => {
    e.preventDefault();
    const trimmed = key.trim();
    if (!trimmed) {
      setError('Paste your API key first.');
      return;
    }
    setSaving(true);
    setError(null);
    try {
      await postHubSettings({ gemini_api_key: trimmed });
      setKey('');
      onConfigured();
    } catch (err) {
      setError(err.message || 'Could not save. Is the backend running?');
    } finally {
      setSaving(false);
    }
  };

  return (
    <div className="first-run">
      <div className="first-run-card">
        <h1 className="first-run-title">Welcome to OmniBot</h1>
        <p className="first-run-lead">
          Add your <strong>Google Gemini API key</strong> once. It is stored on this machine in{' '}
          <code className="first-run-code">hub_secrets.json</code> — you do not need to create any files by hand.
        </p>
        <p className="first-run-hint">
          Get a key free from{' '}
          <a href={AI_STUDIO_URL} target="_blank" rel="noreferrer">
            Google AI Studio
          </a>
          .
        </p>

        <form className="first-run-form" onSubmit={handleSubmit}>
          <label className="first-run-label" htmlFor="first-run-key">
            Gemini API key
          </label>
          <input
            id="first-run-key"
            type="password"
            autoComplete="off"
            className="first-run-input"
            value={key}
            onChange={(e) => setKey(e.target.value)}
            placeholder="Paste your API key"
            disabled={saving}
          />
          {error && <p className="first-run-error">{error}</p>}
          <button type="submit" className="first-run-submit" disabled={saving}>
            {saving ? 'Saving…' : 'Continue'}
          </button>
        </form>

        <p className="first-run-footnote">
          You can change or add Maps keys later under <strong>Hub settings</strong>. Environment variables still override
          file storage if you use Docker or advanced setups.
        </p>
      </div>
    </div>
  );
}
