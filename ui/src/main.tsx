import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';

import { App } from './App';
import { seedMockBackend } from './bridge/mockBackend';
import { initialiseSettings } from './settings';
import './styles/tokens.css';

// Before the first render, so no control flashes at zero. A no-op when a real
// plugin is behind the page.
seedMockBackend();

// Also before the first render: publishes the stored theme and the motion and
// glow settings onto <html>. Doing it in an effect instead would paint one
// frame in the default theme, with animations the user has turned off - which
// is exactly the flash the setting exists to avoid.
initialiseSettings();

const container = document.getElementById('root');

if (!container) {
  throw new Error('GNARL: #root missing from index.html');
}

createRoot(container).render(
  <StrictMode>
    <App />
  </StrictMode>,
);

// A plugin window is not a browser tab: suppress the default context menu so
// right-click is free for modulation assignment and MIDI learn.
window.addEventListener('contextmenu', (e) => e.preventDefault());
