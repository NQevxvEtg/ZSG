//@version=5

strategy('Dynamic Supply & Demand Adaptive Moving Averages with Regime Logic', 'DSDAMARL', overlay=true, commission_type=strategy.commission.percent, commission_value=0.05, initial_capital=1000, default_qty_type=strategy.percent_of_equity, default_qty_value=14.5, currency=currency.USD)


// Start code here :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::


// === Inputs ===

// Moving Average Settings
fastMaLength = input.int(12, title="Base Fast MA Length", minval=1)
slowMaLength = input.int(26, title="Base Slow MA Length", minval=1)

// Faith Index Settings
trust_length = input.int(288, title="Faith Trust Length (Periods)")
resilience_threshold = input.float(2.0, title="Resilience Threshold")
content_zone_multiplier = input.float(1.0, title="Content Zone Multiplier")
purpose_weight = input.float(1.0, title="Purpose Alignment Weight")
correlation_length = input.int(288, title="Correlation Length (Periods)")

// Regime Detection Settings
trendThresholdStrong = input.float(35.0, title="Strong Trend Threshold (ADX)")
trendThresholdWeak = input.float(15.0, title="Weak Trend Threshold (ADX)")
regimeSwitchLength = input.int(14, title="Regime Detection Length")

// OBV Settings
obvMaLength = input.int(14, title="OBV MA Length")

// === Core Calculations ===

// Faith Index
sma_value = ta.sma(close, trust_length)
volatility = ta.stdev(close, trust_length)
trust_score = 1 - (volatility / sma_value)

deviation = math.abs(close - sma_value)
is_outlier = deviation > (resilience_threshold * volatility)
resilience_score = 1 - math.sum(is_outlier ? 1 : 0, trust_length) / trust_length

upper_zone = sma_value + (content_zone_multiplier * volatility)
lower_zone = sma_value - (content_zone_multiplier * volatility)
content_score = 1 - math.max(0, math.abs(close - sma_value) / (upper_zone - lower_zone))

purpose_score = purpose_weight * ta.correlation(close, sma_value, correlation_length)

faith_index = (trust_score + resilience_score + content_score + purpose_score) / 4

// Regime Detection
TrueRange = ta.tr(true)
DirectionalMovementPlus = high - high[1] > low[1] - low ? math.max(high - high[1], 0) : 0
DirectionalMovementMinus = low[1] - low > high - high[1] ? math.max(low[1] - low, 0) : 0

SmoothedTrueRange = ta.rma(TrueRange, regimeSwitchLength)
SmoothedDirectionalMovementPlus = ta.rma(DirectionalMovementPlus, regimeSwitchLength)
SmoothedDirectionalMovementMinus = ta.rma(DirectionalMovementMinus, regimeSwitchLength)

DIPlus = SmoothedTrueRange != 0 ? SmoothedDirectionalMovementPlus / SmoothedTrueRange * 100 : 0
DIMinus = SmoothedTrueRange != 0 ? SmoothedDirectionalMovementMinus / SmoothedTrueRange * 100 : 0

sumDI = DIPlus + DIMinus
dx = sumDI != 0 ? math.abs(DIPlus - DIMinus) / sumDI * 100 : 0
ADX = ta.rma(dx, regimeSwitchLength)

// Regime Logic Hierarchy
regime = ADX > trendThresholdStrong ? "Strong Trend" : ADX > trendThresholdWeak ? "Weak Trend" : "No Trend"

// OBV
obv = ta.cum(ta.change(close) > 0 ? volume : ta.change(close) < 0 ? -volume : 0)
obvSignal = ta.sma(obv, obvMaLength)

// === Adaptive EMA Calculations ===

// Base smoothing factors
baseFastAlpha = 2 / (fastMaLength + 1)
baseSlowAlpha = 2 / (slowMaLength + 1)

// Gradient Adjustment
trendStrength = regime == "Strong Trend" ? 1 : regime == "Weak Trend" ? 0.5 : 0.2
fastAlpha = baseFastAlpha * (1 + faith_index * trendStrength)
slowAlpha = baseSlowAlpha * (1 - faith_index * trendStrength)

// Clamp alpha values
fastAlpha := math.min(math.max(fastAlpha, 0.01), 1)
slowAlpha := math.min(math.max(slowAlpha, 0.01), 1)

// Manually compute adaptive EMAs
var float fastMA = na
var float slowMA = na

fastMA := na(fastMA[1]) ? close : fastAlpha * close + (1 - fastAlpha) * fastMA[1]
slowMA := na(slowMA[1]) ? close : slowAlpha * close + (1 - slowAlpha) * slowMA[1]

// === Plotting ===

plot(fastMA, title="Fast Adaptive MA", color=color.green, linewidth=2)
plot(slowMA, title="Slow Adaptive MA", color=color.red, linewidth=2)

fill(plot1=plot(fastMA, display=display.none), plot2=plot(slowMA, display=display.none), color=fastMA > slowMA ? color.new(color.green, 80) : color.new(color.red, 80), title="MA Fill")

bullishCross = ta.crossover(fastMA, slowMA)
bearishCross = ta.crossunder(fastMA, slowMA)

plotshape(bullishCross, title="Bullish Crossover", location=location.belowbar, color=color.green, style=shape.triangleup, size=size.tiny)
plotshape(bearishCross, title="Bearish Crossover", location=location.abovebar, color=color.red, style=shape.triangledown, size=size.tiny)


// Indicator ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Long Enrty/Exit
longCondition = bullishCross
closeLong = bearishCross

// Short Enrty/Exit
shortCondition = bearishCross
closeShort = bullishCross



// Enable long / short
longEnabled = input(true)
shortEnabled = input(false)


// Bot web-link alert - {{strategy.order.comment}}
botLONG = ''
botCLOSELONG = ''
botSHORT = ''
botCLOSESHORT = ''

// BACKTEST ENGINE 
//[NO USER INPUT REQUIRED] 

// Time period
testStartYear = input(0, 'Backtest Start Year')
testStartMonth = input(1, 'Backtest Start Month')
testStartDay = input(1, 'Backtest Start Day')
testPeriodStart = timestamp(testStartYear, testStartMonth, testStartDay, 0, 0)

periodLength = input.int(999999, 'Backtest Period (days)', minval=0, tooltip='Days until strategy ends') * 86400000  // convert days into UNIX time
testPeriodStop = testPeriodStart + periodLength

testPeriod() =>
    time >= testPeriodStart and time <= testPeriodStop ? true : false

// Convert Take profit and Stop loss to percentage
longTP = input.float(title='Long Take Profit (%)', minval=0.0, step=0.1, defval=0) * 0.01  // Set levels with input options
longSL = input.float(title='Long Stop Loss (%)', minval=0.0, step=0.1, defval=14.5) * 0.01  // Set levels with input options
shortTP = input.float(title='Short Take Profit (%)', minval=0.0, step=0.1, defval=0) * 0.01  // Set levels with input options
shortSL = input.float(title='Short Stop Loss (%)', minval=0.0, step=0.1, defval=14.5) * 0.01  // Set levels with input options

// 0% TP/SL = OFF (a value of 0 turns off TP/SL feature)
longProfitPerc = longTP == 0 ? 1000 : longTP
longStopPerc = longSL == 0 ? 1 : longSL
shortProfitPerc = shortTP == 0 ? 1 : shortTP
shortStopPerc = shortSL == 0 ? 1000 : shortSL

// Determine TP/SL price based on percentage given
longProfitPrice = strategy.position_avg_price * (1 + longProfitPerc)
longStopPrice = strategy.position_avg_price * (1 - longStopPerc)
shortProfitPrice = strategy.position_avg_price * (1 - shortProfitPerc)
shortStopPrice = strategy.position_avg_price * (1 + shortStopPerc)


// Anti-overlap
isEntry_Long = false
isEntry_Long := nz(isEntry_Long[1], false)
isExit_Long = false
isExit_Long := nz(isExit_Long[1], false)
isEntry_Short = false
isEntry_Short := nz(isEntry_Short[1], false)
isExit_Short = false
isExit_Short := nz(isExit_Short[1], false)

entryLong = not isEntry_Long and longCondition
exitLong = not isExit_Long and closeLong
entryShort = not isEntry_Short and shortCondition
exitShort = not isExit_Short and closeShort

if entryLong
    isEntry_Long := true
    isExit_Long := false
    isExit_Long
if exitLong
    isEntry_Long := false
    isExit_Long := true
    isExit_Long
if entryShort
    isEntry_Short := true
    isExit_Short := false
    isExit_Short
if exitShort
    isEntry_Short := false
    isExit_Short := true
    isExit_Short

// Order Execution
if testPeriod()
    if entryLong and longEnabled
        strategy.entry(id='Long', direction=strategy.long, comment=botLONG)  // {{strategy.order.comment}}
    if entryShort and shortEnabled
        strategy.entry(id='Short', direction=strategy.short, comment=botSHORT)  // {{strategy.order.comment}}



// TP/SL Execution
if strategy.position_size > 0
    strategy.exit(id='Long SL/TP', from_entry='Long', limit=longProfitPrice, stop=longStopPrice)
	if exitLong
    	strategy.close(id='Long', comment=botCLOSELONG)  // {{strategy.order.comment}}

if strategy.position_size < 0
    strategy.exit(id='Short TP/SL', from_entry='Short', limit=shortProfitPrice, stop=shortStopPrice)
	if exitShort
    	strategy.close(id='Short', comment=botCLOSESHORT)  // {{strategy.order.comment}}

// // Draw Entry, TP and SL Levels for Long Positions
// plot(strategy.position_size > 0 ? longTP == 0 ? na : longProfitPrice : na, style=plot.style_linebr, color=color.new(color.green, 0), title='Long TP')
// plot(strategy.position_size > 0 ? strategy.position_avg_price : na, style=plot.style_linebr, color=color.new(color.blue, 0), title='Long Entry')
// plot(strategy.position_size > 0 ? longSL == 0 ? na : longStopPrice : na, style=plot.style_linebr, color=color.new(color.red, 0), title='Long SL')
// // Draw Entry, TP and SL Levels for Short Positions
// plot(strategy.position_size < 0 ? shortTP == 0 ? na : shortProfitPrice : na, style=plot.style_linebr, color=color.new(color.green, 0), title='Short TP')
// plot(strategy.position_size < 0 ? strategy.position_avg_price : na, style=plot.style_linebr, color=color.new(color.blue, 0), title='Short Entry')
// plot(strategy.position_size < 0 ? shortSL == 0 ? na : shortStopPrice : na, style=plot.style_linebr, color=color.new(color.red, 0), title='Short SL')


