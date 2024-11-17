//@version=5
strategy("Fourier Scalping with Clustering", overlay=true, initial_capital=1000, default_qty_type=strategy.percent_of_equity, default_qty_value=100, commission_type=strategy.commission.percent, commission_value=0.05)

// Inputs
cycles = input.int(10, title="Number of Cycles")
lookback = input.int(50, title="Fourier Lookback Period")
base_trade_cooldown = input.int(20, title="Base Trade Cooldown")
volatility_buffer = input.float(0.01, title="Volatility Buffer (Multiplier)")
dynamic_risk_factor_scale = input.float(0.8, title="Dynamic Risk Factor Scaling")
trade_direction = input.string(title="Trade Direction", defval="Both", options=["Long", "Short", "Both"])  // Added option

// Variables
var int last_trade_time = na
var float[] fourier_values = array.new_float(cycles, 0.0)
var float[] fourier_slopes = array.new_float(cycles, 0.0)

// Fourier Transform - Multiple Cycles
for i = 0 to (cycles - 1)
    fourier_component = ta.sma(close * math.cos(2 * math.pi * i * bar_index / lookback), lookback)
    array.set(fourier_values, i, fourier_component)

// Weighted Convergence Using Top Cycles
weighted_convergence = 0.0
weighted_slope = 0.0
weight_sum = 0.0

for i = 0 to (cycles - 1)
    weight = cycles - i  // Higher weights for dominant (lower-frequency) cycles
    weighted_convergence += array.get(fourier_values, i) * weight
    slope = array.get(fourier_values, i) - nz(array.get(fourier_values, i)[1])
    array.set(fourier_slopes, i, slope)  // Store slope changes for each cycle
    weighted_slope += slope * weight
    weight_sum += weight

weighted_convergence /= weight_sum
weighted_slope /= weight_sum

// Volatility Clustering Integration
atr = ta.atr(14)
volatility_perf = ta.ema(math.abs(close - close[1]), lookback)
recent_volatility = ta.stdev(close, lookback)
clustering_adjustment_input = math.min(1.0, math.max(0.1, recent_volatility / atr))
adjustment_factor = 1 - (clustering_adjustment_input * volatility_perf / atr)
adjustment_factor := math.max(0.0, math.min(1.0, adjustment_factor))  // Clamp to [0, 1]

// Apply adjustment to weighted slope
adaptive_slope = weighted_slope * adjustment_factor

// Entry Conditions
price_above_convergence = close > (weighted_convergence + volatility_buffer * atr)
price_below_convergence = close < (weighted_convergence - volatility_buffer * atr)

momentum_filter = ta.sma(close, 10) > ta.sma(close, 20)
long_condition = (adaptive_slope > 0 and price_above_convergence) and (na(last_trade_time) or bar_index - last_trade_time > base_trade_cooldown) and momentum_filter
short_condition = (adaptive_slope < 0 and price_below_convergence) and (na(last_trade_time) or bar_index - last_trade_time > base_trade_cooldown) and not momentum_filter

// Check Trade Direction
long_enabled = (trade_direction == "Long") or (trade_direction == "Both")
short_enabled = (trade_direction == "Short") or (trade_direction == "Both")

// Adaptive Stop-Loss and Take-Profit
dynamic_risk_factor = math.abs(adaptive_slope) + atr * dynamic_risk_factor_scale
long_stop_loss = close - dynamic_risk_factor * math.abs(weighted_slope) * 50
long_take_profit = close + dynamic_risk_factor * math.abs(weighted_slope) * 100

short_stop_loss = close + dynamic_risk_factor * math.abs(weighted_slope) * 50
short_take_profit = close - dynamic_risk_factor * math.abs(weighted_slope) * 100

// Trailing Stop-Loss for Active Positions
trailing_points = dynamic_risk_factor * 50

// Trade Execution
if (long_condition and long_enabled)
    strategy.entry("Buy", strategy.long)
    strategy.exit("Sell", from_entry="Buy", stop=long_stop_loss, limit=long_take_profit, trail_points=trailing_points)
    last_trade_time := bar_index

if (short_condition and short_enabled)
    strategy.entry("Sell", strategy.short)
    strategy.exit("Cover", from_entry="Sell", stop=short_stop_loss, limit=short_take_profit, trail_points=trailing_points)
    last_trade_time := bar_index

// Visualization
plotshape(long_condition and long_enabled, style=shape.triangledown, location=location.belowbar,  color=#F44336, size=size.auto)
plotshape(short_condition and short_enabled, style=shape.triangleup,   location=location.abovebar, color=#009688, size=size.auto)
