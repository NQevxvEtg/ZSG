//@version=5
// by tonymontanov
strategy('Asymmetric Volatility', 'AV', overlay=false, commission_type=strategy.commission.percent, commission_value=0.05, initial_capital=10000, default_qty_type=strategy.percent_of_equity, default_qty_value=23.6, currency=currency.USD)


// Start code here :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::


// Indicator ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// User Inputs
volLengthInput              = input.int(defval = 15, title = "Length", minval = 1, group = "Main settings")
sourceInput                 = input.source(defval = close, title = "Source", group = "Main settings")
measureInput                = input.string(defval = "Bps", title = "Measure", options = ["Bps", "Prc"], group = "Main settings")
useMcGinleyUnput            = input.bool(defval = true, title = "Use McGinley Dynamic smoothing?", group = "McGinley Dynamic smoothing settings")
mcGinleyLengthInput         = input.int(defval = 5, minval = 1, title = "McGinley Dynamic length", group = "McGinley Dynamic smoothing settings")
mcGinleyKInput              = input.float(defval = 0.6, title = "McGinley k value", minval = 0.1, step = 0.05, group = "McGinley Dynamic smoothing settings")
mcGinleyExponentInput       = input.float(defval = 3.0, title = "McGinley exponent", minval = 1.0, step = 0.1, group = "McGinley Dynamic smoothing settings")
clusterLookbackTooltip      = "Adjust the lookback period for clustering strength. A higher value makes the indicator more sensitive to volatility clustering."
clusterLookbackInput        = input.int(defval = 1, title = "Cluster lookback period", minval = 1, step = 1, tooltip = clusterLookbackTooltip, group = "McGinley Dynamic smoothing settings")
clusteringAdjustmentInput   = input.float(defval = 0, title = "Clustering Adjustment Factor", step = 0.05, minval = 0.0, maxval = 1.0, group = "McGinley Dynamic smoothing settings")

// Function Definitions
AsymetricVolatility(source, length)=>
    upMoves           = math.max(source - source[1], 0)
    downMoves         = math.max(source[1] - source, 0)
    totalMoves        = math.sum(math.abs(source - source[1]), length) 
    upVolatility      = math.sum(upMoves, length) / length
    downVolatility    = math.sum(downMoves, length) / length
    upVolatilityPrc   = upVolatility / totalMoves * 20
    downVolatilityPrc = downVolatility / totalMoves * 20
    [upVolatilityPrc, downVolatilityPrc]

McGinleyDynamic(source, fPeriod, fK, fExponent)=>
    period  = math.max(1.0, fPeriod)
    md      = 0.0
    priorMd = nz(md[1], source)
    md      := priorMd + (source - priorMd) / math.min(period, math.max(1.0, fK * period * math.pow(source / priorMd, fExponent)))
    md

// Calculations
float upVolatility   = na
float downVolatility = na

if measureInput == "Bps"
    upMoves   = math.max(sourceInput - sourceInput[1], 0)
    downMoves = math.max(sourceInput[1] - sourceInput, 0)
    upVolatility    := math.sum(upMoves, volLengthInput) / volLengthInput
    downVolatility  := math.sum(downMoves, volLengthInput) / volLengthInput
else
    [upVolatilityPrc, downVolatilityPrc] = AsymetricVolatility(sourceInput, volLengthInput)
    upVolatility   := upVolatilityPrc * 2
    downVolatility := downVolatilityPrc * 2

if useMcGinleyUnput
    // McGinley Dynamic smoothing
    mcGinleyUpVolatility = McGinleyDynamic(upVolatility, mcGinleyLengthInput, mcGinleyKInput, mcGinleyExponentInput)
    mcGinleyDownVolatility = McGinleyDynamic(downVolatility, mcGinleyLengthInput, mcGinleyKInput, mcGinleyExponentInput)
    
    // Calculate volatility performance
    volatilityPerf = ta.ema(math.abs(sourceInput - sourceInput[1]), clusterLookbackInput)
    
    // Revised adjustment factor calculation
    adjustmentFactor = 1 - (clusteringAdjustmentInput * volatilityPerf / 100)
    adjustmentFactor := math.max(0.0, math.min(1.0, adjustmentFactor))  // Ensure it's between 0 and 1
    
    // Apply the adjustment factor
    adjustedMcGinleyUpVolatility   = mcGinleyUpVolatility * adjustmentFactor
    adjustedMcGinleyDownVolatility = mcGinleyDownVolatility * adjustmentFactor
    
    // Use the adjusted values for plotting
    upVolatility   := adjustedMcGinleyUpVolatility
    downVolatility := adjustedMcGinleyDownVolatility

// Plotting
backgroundColor = upVolatility > downVolatility ? color.rgb(60, 166, 75, 90) : color.rgb(178, 24, 44, 90)

plot(upVolatility, title = "Upward volatility", color = color.rgb(60, 166, 75), style = plot.style_stepline)
plot(downVolatility, title = "Downward volatility", color = color.rgb(178, 24, 44), style = plot.style_stepline)
bgcolor(color = backgroundColor, title = "Background")


// Indicator ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Long Enrty/Exit
longCondition = upVolatility > downVolatility
closeLong = upVolatility < downVolatility

// Short Enrty/Exit
shortCondition = upVolatility < downVolatility
closeShort = upVolatility > downVolatility



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
longSL = input.float(title='Long Stop Loss (%)', minval=0.0, step=0.1, defval=0) * 0.01  // Set levels with input options
shortTP = input.float(title='Short Take Profit (%)', minval=0.0, step=0.1, defval=0) * 0.01  // Set levels with input options
shortSL = input.float(title='Short Stop Loss (%)', minval=0.0, step=0.1, defval=0) * 0.01  // Set levels with input options

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
