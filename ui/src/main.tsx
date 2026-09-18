import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';

import { App } from './App';
import { seedMockBackend } from './bridge/mockBackend';
import './styles/tokens.css';

// Before the first render, so no control flashes at zero. A no-op when a real
// plugin is behind the page.
seedMockBackend();

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
