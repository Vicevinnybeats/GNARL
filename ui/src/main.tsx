import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';

import { App } from './App';
import './styles/tokens.css';

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
