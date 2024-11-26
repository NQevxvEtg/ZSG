//@version=5

strategy('Dynamic Supply & Demand Adaptive Moving Averages with Regime Logic', 'DSDAMARL', overlay=true, commission_type=strategy.commission.percent, commission_value=0.05, initial_capital=1000, default_qty_type=strategy.percent_of_equity, default_qty_value=14.5, currency=currency.USD)


// Start code here :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::


// === Inputs ===
fastMaLength = input.int(12, title="Fast MA Length", minval=1)
slowMaLength = input.int(26, title="Slow MA Length", minval=1)
trendThresholdStrong = input.float(35.0, title="Strong Trend Threshold (ADX)")
trendThresholdWeak = input.float(15.0, title="Weak Trend Threshold (ADX)")
regimeSwitchLength = input.int(14, title="Regime Detection Length")
faithTrustLength = input.int(288, title="Faith Trust Length (Periods)")

// === Core Functions ===

// Regime Logic
f_regime_logic(trendThresholdStrong, trendThresholdWeak, regimeSwitchLength) =>
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
    ADX > trendThresholdStrong ? "Strong Trend" : ADX > trendThresholdWeak ? "Weak Trend" : "No Trend"

// Faith Index
f_faith_index(trustLength) =>
    smaValue = ta.sma(close, trustLength)
    volatility = ta.stdev(close, trustLength)
    resilienceScore = 1 - (volatility / smaValue)
    contentZone = math.abs(close - smaValue) / (2 * volatility)
    trustScore = 1 - math.min(contentZone, 1)
    (resilienceScore + trustScore) / 2

// Regime-Specific Moving Averages

// Trending Market (EMA)
f_trending_ma(length) =>
    ta.ema(close, length)

// Ranging Market (SMA)
f_ranging_ma(length) =>
    ta.sma(close, length)

// Volatile Market (VWMA)
f_volatile_ma(length) =>
    ta.vwma(close, length)

// Controller Logic
f_brain(trendThresholdStrong, trendThresholdWeak, regimeSwitchLength, fastLength, slowLength) =>
    // Regime determination
    regime = f_regime_logic(trendThresholdStrong, trendThresholdWeak, regimeSwitchLength)
    faithIndex = f_faith_index(faithTrustLength)

    // Initialize outputs
    float fastMA = na
    float slowMA = na

    // Regime-Specific Moving Averages
    if regime == "Strong Trend"
        fastMA := f_trending_ma(fastLength)
        slowMA := f_trending_ma(slowLength)
    else if regime == "Weak Trend"
        fastMA := f_ranging_ma(fastLength)
        slowMA := f_ranging_ma(slowLength)
    else  // No Trend (Volatile Market)
        fastMA := f_volatile_ma(fastLength)
        slowMA := f_volatile_ma(slowLength)

    // Return the dynamic MAs
    [fastMA, slowMA]

// === Execution ===

// Compute moving averages dynamically
[fastDynamicMA, slowDynamicMA] = f_brain(trendThresholdStrong, trendThresholdWeak, regimeSwitchLength, fastMaLength, slowMaLength)

// === Plotting ===
plot(fastDynamicMA, title="Fast Dynamic MA", color=color.green, linewidth=2)
plot(slowDynamicMA, title="Slow Dynamic MA", color=color.red, linewidth=2)

// Fill between MAs
fill(plot1=plot(fastDynamicMA, display=display.none), plot2=plot(slowDynamicMA, display=display.none),color=fastDynamicMA > slowDynamicMA ? color.new(color.green, 80) : color.new(color.red, 80), title="MA Fill")

// Signals based on MA crossovers
bullishCross = ta.crossover(fastDynamicMA, slowDynamicMA)
bearishCross = ta.crossunder(fastDynamicMA, slowDynamicMA)

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


