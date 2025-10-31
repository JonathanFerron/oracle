import React, { useState } from 'react';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, ReferenceLine, Cell } from 'recharts';

const DiceMaxDistribution = () => {
  const [n, setN] = useState(8);
  
  // Calculate probability distribution
  const calculateDistribution = (sides) => {
    const data = [];
    for (let k = 1; k <= sides; k++) {
      const prob = (2 * k - 1) / (sides * sides);
      data.push({
        k,
        probability: prob,
        percentage: (prob * 100).toFixed(2)
      });
    }
    return data;
  };
  
  // Calculate statistics
  const calculateStats = (sides) => {
    const mean = (2 * sides * sides + 3 * sides + 1) / (3 * sides);
    const variance = (sides * sides - 1) / 18;
    const stdDev = Math.sqrt(variance);
    
    // Mode is always the maximum value
    const mode = sides;
    const modeProb = (2 * sides - 1) / (sides * sides);
    
    return { mean, variance, stdDev, mode, modeProb };
  };
  
  const data = calculateDistribution(n);
  const stats = calculateStats(n);
  
  // Find which bar corresponds to mode
  const getModeIndex = (k) => k === stats.mode;
  
  const CustomTooltip = ({ active, payload }) => {
    if (active && payload && payload.length) {
      return (
        <div className="bg-white p-3 border-2 border-gray-300 rounded shadow-lg">
          <p className="font-semibold">Value: {payload[0].payload.k}</p>
          <p className="text-blue-600">Probability: {payload[0].payload.probability.toFixed(4)}</p>
          <p className="text-blue-600">Percentage: {payload[0].payload.percentage}%</p>
        </div>
      );
    }
    return null;
  };

  return (
    <div className="w-full h-full p-6 bg-gradient-to-br from-blue-50 to-indigo-50">
      <div className="max-w-6xl mx-auto">
        <h1 className="text-3xl font-bold text-center mb-2 text-indigo-900">
          Maximum of Two d{n} Dice Rolls
        </h1>
        <p className="text-center text-gray-600 mb-6">
          M = max(d₁({n}), d₂({n}))
        </p>
        
        {/* Dice selector */}
        <div className="bg-white rounded-lg shadow-md p-4 mb-6">
          <label className="block text-sm font-medium text-gray-700 mb-2">
            Select Die Type (d4 to d20):
          </label>
          <div className="flex items-center gap-4">
            <input
              type="range"
              min="4"
              max="20"
              value={n}
              onChange={(e) => setN(parseInt(e.target.value))}
              className="flex-grow"
            />
            <div className="text-2xl font-bold text-indigo-600 w-16 text-center">
              d{n}
            </div>
          </div>
          <div className="flex justify-between text-xs text-gray-500 mt-1 px-1">
            <span>d4</span>
            <span>d6</span>
            <span>d8</span>
            <span>d10</span>
            <span>d12</span>
            <span>d20</span>
          </div>
        </div>

        {/* Statistics panel */}
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-6">
          <div className="bg-white rounded-lg shadow-md p-4">
            <div className="text-sm text-gray-600">Mean (μ)</div>
            <div className="text-2xl font-bold text-green-600">
              {stats.mean.toFixed(3)}
            </div>
          </div>
          <div className="bg-white rounded-lg shadow-md p-4">
            <div className="text-sm text-gray-600">Std Dev (σ)</div>
            <div className="text-2xl font-bold text-orange-600">
              {stats.stdDev.toFixed(3)}
            </div>
          </div>
          <div className="bg-white rounded-lg shadow-md p-4">
            <div className="text-sm text-gray-600">Mode</div>
            <div className="text-2xl font-bold text-purple-600">
              {stats.mode}
            </div>
          </div>
          <div className="bg-white rounded-lg shadow-md p-4">
            <div className="text-sm text-gray-600">Variance</div>
            <div className="text-2xl font-bold text-blue-600">
              {stats.variance.toFixed(3)}
            </div>
          </div>
        </div>

        {/* Chart */}
        <div className="bg-white rounded-lg shadow-md p-6">
          <ResponsiveContainer width="100%" height={400}>
            <BarChart data={data} margin={{ top: 20, right: 30, left: 20, bottom: 20 }}>
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis 
                dataKey="k" 
                label={{ value: 'Maximum Value (k)', position: 'insideBottom', offset: -10 }}
              />
              <YAxis 
                label={{ value: 'Probability', angle: -90, position: 'insideLeft' }}
              />
              <Tooltip content={<CustomTooltip />} />
              <Legend />
              
              {/* Mean line */}
              <ReferenceLine 
                x={stats.mean} 
                stroke="green" 
                strokeWidth={2}
                strokeDasharray="5 5"
                label={{ value: `μ = ${stats.mean.toFixed(2)}`, position: 'top', fill: 'green', fontWeight: 'bold' }}
              />
              
              {/* Mean - 1 std dev */}
              <ReferenceLine 
                x={stats.mean - stats.stdDev} 
                stroke="orange" 
                strokeWidth={1.5}
                strokeDasharray="3 3"
                label={{ value: 'μ - σ', position: 'top', fill: 'orange', fontSize: 11 }}
              />
              
              {/* Mean + 1 std dev */}
              <ReferenceLine 
                x={stats.mean + stats.stdDev} 
                stroke="orange" 
                strokeWidth={1.5}
                strokeDasharray="3 3"
                label={{ value: 'μ + σ', position: 'top', fill: 'orange', fontSize: 11 }}
              />
              
              <Bar dataKey="probability" name="P(M = k)">
                {data.map((entry, index) => (
                  <Cell 
                    key={`cell-${index}`} 
                    fill={getModeIndex(entry.k) ? '#9333ea' : '#3b82f6'} 
                  />
                ))}
              </Bar>
            </BarChart>
          </ResponsiveContainer>
          
          <div className="mt-4 text-sm text-gray-600 space-y-1">
            <div className="flex items-center gap-2">
              <div className="w-4 h-4 bg-blue-500"></div>
              <span>Probability distribution bars</span>
            </div>
            <div className="flex items-center gap-2">
              <div className="w-4 h-4 bg-purple-600"></div>
              <span>Mode (most likely value)</span>
            </div>
            <div className="flex items-center gap-2">
              <div className="w-8 h-0.5 bg-green-600"></div>
              <span>Mean (μ = {stats.mean.toFixed(3)})</span>
            </div>
            <div className="flex items-center gap-2">
              <div className="w-8 h-0.5 bg-orange-600" style={{borderTop: '2px dashed'}}></div>
              <span>± One standard deviation (σ = {stats.stdDev.toFixed(3)})</span>
            </div>
          </div>
        </div>

        {/* Formulas */}
        <div className="bg-white rounded-lg shadow-md p-6 mt-6">
          <h2 className="text-xl font-bold mb-4 text-indigo-900">Formulas</h2>
          <div className="space-y-3 text-sm font-mono">
            <div className="bg-gray-50 p-3 rounded">
              <div className="font-semibold text-gray-700 mb-1">Probability Mass Function:</div>
              <div>P(M = k) = (2k - 1) / n²</div>
            </div>
            <div className="bg-gray-50 p-3 rounded">
              <div className="font-semibold text-gray-700 mb-1">Expectation:</div>
              <div>E[M] = (2n² + 3n + 1) / (3n)</div>
            </div>
            <div className="bg-gray-50 p-3 rounded">
              <div className="font-semibold text-gray-700 mb-1">Variance:</div>
              <div>Var(M) = (n² - 1) / 18</div>
            </div>
            <div className="bg-gray-50 p-3 rounded">
              <div className="font-semibold text-gray-700 mb-1">Standard Deviation:</div>
              <div>σ(M) = √[(n² - 1) / 18]</div>
            </div>
          </div>
        </div>

        {/* Distribution table */}
        <div className="bg-white rounded-lg shadow-md p-6 mt-6">
          <h2 className="text-xl font-bold mb-4 text-indigo-900">Probability Distribution Table for d{n}</h2>
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="bg-indigo-100">
                  <th className="p-2 text-left">k</th>
                  <th className="p-2 text-right">P(M = k)</th>
                  <th className="p-2 text-right">Decimal</th>
                  <th className="p-2 text-right">Percentage</th>
                  <th className="p-2 text-right">Cumulative</th>
                </tr>
              </thead>
              <tbody>
                {data.map((row, idx) => {
                  const cumulative = data.slice(0, idx + 1).reduce((sum, r) => sum + r.probability, 0);
                  const isMode = row.k === stats.mode;
                  return (
                    <tr key={row.k} className={`border-b ${isMode ? 'bg-purple-50 font-semibold' : ''}`}>
                      <td className="p-2">{row.k}</td>
                      <td className="p-2 text-right font-mono">
                        {(2 * row.k - 1)}/{n * n}
                      </td>
                      <td className="p-2 text-right font-mono">
                        {row.probability.toFixed(4)}
                      </td>
                      <td className="p-2 text-right">{row.percentage}%</td>
                      <td className="p-2 text-right font-mono">{cumulative.toFixed(4)}</td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        </div>
      </div>
    </div>
  );
};

export default DiceMaxDistribution;
