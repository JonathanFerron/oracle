import React, { useState } from 'react';
import { Download } from 'lucide-react';

const OrderSymbolsGenerator = () => {
  const [selectedSymbol, setSelectedSymbol] = useState('sun');
  
  const symbols = {
    sun: {
      name: 'Order A - Dawn Light',
      unicode: '☀',
      color: '#FFA500',
      svg: `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <!-- Sun rays -->
  <line x1="50" y1="10" x2="50" y2="25" stroke="#FFA500" stroke-width="4" stroke-linecap="round"/>
  <line x1="50" y1="75" x2="50" y2="90" stroke="#FFA500" stroke-width="4" stroke-linecap="round"/>
  <line x1="10" y1="50" x2="25" y2="50" stroke="#FFA500" stroke-width="4" stroke-linecap="round"/>
  <line x1="75" y1="50" x2="90" y2="50" stroke="#FFA500" stroke-width="4" stroke-linecap="round"/>
  <line x1="21" y1="21" x2="32" y2="32" stroke="#FFA500" stroke-width="4" stroke-linecap="round"/>
  <line x1="68" y1="68" x2="79" y2="79" stroke="#FFA500" stroke-width="4" stroke-linecap="round"/>
  <line x1="79" y1="21" x2="68" y2="32" stroke="#FFA500" stroke-width="4" stroke-linecap="round"/>
  <line x1="32" y1="68" x2="21" y2="79" stroke="#FFA500" stroke-width="4" stroke-linecap="round"/>
  <!-- Center circle -->
  <circle cx="50" cy="50" r="18" fill="none" stroke="#FFA500" stroke-width="4"/>
</svg>`
    },
    leaf: {
      name: 'Order B - Verdant Light',
      unicode: '🍃',
      color: '#228B22',
      svg: `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <!-- Leaf outline -->
  <path d="M 50 15 Q 70 30 75 50 Q 70 70 50 85 Q 30 70 25 50 Q 30 30 50 15 Z" 
        fill="none" stroke="#228B22" stroke-width="4" stroke-linejoin="round"/>
  <!-- Center vein -->
  <line x1="50" y1="15" x2="50" y2="85" stroke="#228B22" stroke-width="3"/>
  <!-- Side veins -->
  <line x1="50" y1="30" x2="35" y2="40" stroke="#228B22" stroke-width="2"/>
  <line x1="50" y1="30" x2="65" y2="40" stroke="#228B22" stroke-width="2"/>
  <line x1="50" y1="50" x2="30" y2="55" stroke="#228B22" stroke-width="2"/>
  <line x1="50" y1="50" x2="70" y2="55" stroke="#228B22" stroke-width="2"/>
  <line x1="50" y1="70" x2="35" y2="72" stroke="#228B22" stroke-width="2"/>
  <line x1="50" y1="70" x2="65" y2="72" stroke="#228B22" stroke-width="2"/>
</svg>`
    },
    flame: {
      name: 'Order C - Ember Light',
      unicode: '🔥',
      color: '#DC143C',
      svg: `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <!-- Outer flame -->
  <path d="M 50 15 Q 60 30 65 45 Q 68 60 65 70 Q 60 80 50 85 Q 40 80 35 70 Q 32 60 35 45 Q 40 30 50 15 Z" 
        fill="none" stroke="#DC143C" stroke-width="4" stroke-linejoin="round"/>
  <!-- Inner flame -->
  <path d="M 50 30 Q 56 40 58 52 Q 58 62 50 70 Q 42 62 42 52 Q 44 40 50 30 Z" 
        fill="none" stroke="#FF6347" stroke-width="3" stroke-linejoin="round"/>
  <!-- Flame wisp at top -->
  <path d="M 50 15 Q 48 10 50 5" fill="none" stroke="#DC143C" stroke-width="2" stroke-linecap="round"/>
</svg>`
    },
    star: {
      name: 'Order D - Eternal Light',
      unicode: '⭐',
      color: '#9932CC',
      svg: `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <!-- 4-pointed star -->
  <path d="M 50 10 L 55 45 L 90 50 L 55 55 L 50 90 L 45 55 L 10 50 L 45 45 Z" 
        fill="none" stroke="#9932CC" stroke-width="4" stroke-linejoin="miter"/>
  <!-- Inner diamond for depth -->
  <path d="M 50 30 L 60 50 L 50 70 L 40 50 Z" 
        fill="none" stroke="#9932CC" stroke-width="2"/>
</svg>`
    },
    moon: {
      name: 'Order E - Moonlight',
      unicode: '🌙',
      color: '#028eb5',
      svg: `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <!-- Crescent moon - outer arc -->
  <path d="M 30 10 C 15 20, 10 40, 10 50 C 10 60, 15 80, 30 90" 
        fill="none" stroke="#028eb5" stroke-width="5" stroke-linecap="round"/>
  <!-- Crescent moon - inner arc -->
  <path d="M 30 10 C 50 25, 60 40, 60 50 C 60 60, 50 75, 30 90" 
        fill="none" stroke="#028eb5" stroke-width="5" stroke-linecap="round"/>
</svg>`
    }
  };

  const downloadSVG = (symbolKey) => {
    const symbol = symbols[symbolKey];
    const blob = new Blob([symbol.svg], { type: 'image/svg+xml' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `order_${symbolKey}.svg`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const downloadAll = () => {
    Object.keys(symbols).forEach(key => {
      setTimeout(() => downloadSVG(key), 100 * Object.keys(symbols).indexOf(key));
    });
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-indigo-900 via-purple-900 to-pink-900 p-8">
      <div className="max-w-6xl mx-auto">
        <div className="bg-white/10 backdrop-blur-lg rounded-lg p-8 mb-8">
          <h1 className="text-4xl font-bold text-white mb-2">Oracle Order Symbols</h1>
          <p className="text-white/80 mb-4">Simple SVG line art for CLI, TUI, GUI, and physical cards (6mm size)</p>
          <button
            onClick={downloadAll}
            className="bg-green-500 hover:bg-green-600 text-white px-6 py-2 rounded-lg flex items-center gap-2 transition-colors"
          >
            <Download size={20} />
            Download All SVGs
          </button>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 mb-8">
          {Object.entries(symbols).map(([key, symbol]) => (
            <div
              key={key}
              className={`bg-white/10 backdrop-blur-lg rounded-lg p-6 cursor-pointer transition-all hover:bg-white/20 ${
                selectedSymbol === key ? 'ring-4 ring-white/50' : ''
              }`}
              onClick={() => setSelectedSymbol(key)}
            >
              <div className="bg-white rounded-lg p-6 mb-4 flex items-center justify-center h-48">
                <div dangerouslySetInnerHTML={{ __html: symbol.svg }} className="w-32 h-32" />
              </div>
              <h3 className="text-xl font-bold text-white mb-2">{symbol.name}</h3>
              <div className="flex items-center justify-between text-white/80 mb-3">
                <span className="text-3xl">{symbol.unicode}</span>
                <span 
                  className="px-3 py-1 rounded"
                  style={{ backgroundColor: symbol.color }}
                >
                  {symbol.color}
                </span>
              </div>
              <button
                onClick={(e) => {
                  e.stopPropagation();
                  downloadSVG(key);
                }}
                className="w-full bg-blue-500 hover:bg-blue-600 text-white px-4 py-2 rounded flex items-center justify-center gap-2 transition-colors"
              >
                <Download size={16} />
                Download SVG
              </button>
            </div>
          ))}
        </div>

        <div className="bg-white/10 backdrop-blur-lg rounded-lg p-8">
          <h2 className="text-2xl font-bold text-white mb-4">Unicode Characters for CLI/TUI</h2>
          <div className="bg-gray-900 rounded-lg p-6 font-mono text-white">
            <div className="grid grid-cols-1 gap-2">
              <div className="flex items-center gap-4">
                <span className="text-2xl w-12">☀</span>
                <code className="text-green-400">U+2600</code>
                <span className="text-white/80">Order A - Dawn Light</span>
              </div>
              <div className="flex items-center gap-4">
                <span className="text-2xl w-12">🍃</span>
                <code className="text-green-400">U+1F343</code>
                <span className="text-white/80">Order B - Verdant Light</span>
              </div>
              <div className="flex items-center gap-4">
                <span className="text-2xl w-12">🔥</span>
                <code className="text-green-400">U+1F525</code>
                <span className="text-white/80">Order C - Ember Light</span>
              </div>
              <div className="flex items-center gap-4">
                <span className="text-2xl w-12">⭐</span>
                <code className="text-green-400">U+2B50</code>
                <span className="text-white/80">Order D - Eternal Light</span>
              </div>
              <div className="flex items-center gap-4">
                <span className="text-2xl w-12">🌙</span>
                <code className="text-green-400">U+1F319</code>
                <span className="text-white/80">Order E - Moonlight</span>
              </div>
            </div>
          </div>
        </div>

        <div className="bg-white/10 backdrop-blur-lg rounded-lg p-8 mt-8">
          <h2 className="text-2xl font-bold text-white mb-4">Implementation Notes</h2>
          <div className="text-white/80 space-y-3">
            <p><strong className="text-white">Physical Cards:</strong> SVGs scale perfectly to 6mm (approximately 17 pixels at 72 DPI or 23 pixels at 96 DPI)</p>
            <p><strong className="text-white">Line Width:</strong> stroke-width="4" in 100x100 viewBox = clean lines at small sizes</p>
            <p><strong className="text-white">Colors:</strong> Customizable via stroke attribute - match to your card color scheme</p>
            <p><strong className="text-white">Format:</strong> Plain SVG with no dependencies - works everywhere</p>
            <p><strong className="text-white">CLI/TUI:</strong> Use the Unicode characters shown above (may need UTF-8 terminal support)</p>
          </div>
        </div>

        <div className="bg-white/10 backdrop-blur-lg rounded-lg p-8 mt-8">
          <h2 className="text-2xl font-bold text-white mb-4">Species by Order Reference</h2>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4 text-white/90">
            <div className="bg-white/5 rounded p-4">
              <div className="flex items-center gap-2 mb-2">
                <span className="text-2xl">☀</span>
                <strong>Order A - Dawn Light</strong>
              </div>
              <p className="text-sm text-white/70">Human, Elf, Dwarf</p>
            </div>
            <div className="bg-white/5 rounded p-4">
              <div className="flex items-center gap-2 mb-2">
                <span className="text-2xl">🍃</span>
                <strong>Order B - Verdant Light</strong>
              </div>
              <p className="text-sm text-white/70">Hobbit, Faun, Centaur</p>
            </div>
            <div className="bg-white/5 rounded p-4">
              <div className="flex items-center gap-2 mb-2">
                <span className="text-2xl">🔥</span>
                <strong>Order C - Ember Light</strong>
              </div>
              <p className="text-sm text-white/70">Orc, Goblin, Minotaur</p>
            </div>
            <div className="bg-white/5 rounded p-4">
              <div className="flex items-center gap-2 mb-2">
                <span className="text-2xl">⭐</span>
                <strong>Order D - Eternal Light</strong>
              </div>
              <p className="text-sm text-white/70">Dragon, Cyclops, Fairy</p>
            </div>
            <div className="bg-white/5 rounded p-4">
              <div className="flex items-center gap-2 mb-2">
                <span className="text-2xl">🌙</span>
                <strong>Order E - Moonlight</strong>
              </div>
              <p className="text-sm text-white/70">Aven, Koatl, Lycan</p>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default OrderSymbolsGenerator;