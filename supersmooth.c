//@version=5
strategy('Supersmooth', 'SuperS', overlay=true, commission_type=strategy.commission.percent, commission_value=0.05, initial_capital=1000, default_qty_type=strategy.percent_of_equity, default_qty_value=23.6, currency=currency.USD)

// Start code here :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

// Indicator ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Original Parameters
atr_length = input.int(title="ATR Length", defval=14, minval=1)
cluster_length = input.int(10, title="Cluster Lookback Period")
perf_memory = input.float(10, title="Performance Memory", minval=2)
supertrend_multiplier = input.float(6.0, title='Supertrend Multiplier', step=0.1, minval=0.1)

// User input for McGinley Dynamic
mcginley_period = input.float(14, title='McGinley Period', minval=1, step=0.5)
mcginley_k = input.float(0.6, title='McGinley K Value', minval=0.1, step=0.05)
mcginley_exponent = input.float(2.0, title='McGinley Exponent', minval=1.0, step=0.1)

// Cosine Weighted Parameters
ma_length = input.int(14, "CWMA Length", inline = "A")
atr_type = input.string("Cosine Weighted ATR", "ATR Type", options=["Normal ATR", "Cosine Weighted ATR"], inline = "A")
cwma_src = input.string("close", "Source", options=["open", "high", "low", "close", "oc2", "hl2", "occ3", "hlc3", "ohlc4", "hlcc4"], group="Cosine Weighted")

// Clustering Adjustment Factor
clustering_adjustment_factor = input.float(0.85, title='Clustering Adjustment Factor', step=0.05, minval=0.5, tooltip='Adjust the strength of the clustering influence (lower values increase clustering impact)')

// Fetching the price data
resCustom = input.timeframe(title='Timeframe', defval='')
price = request.security(syminfo.tickerid, resCustom, close)

// Partial Close Percentage Input
partial_close_percent = input.float(title="Partial Close Percentage", defval=50.0, minval=1.0, maxval=100.0, tooltip="Percentage of position to close during partial exit")

// Function for Cosine Weighted Moving Average
f_Cosine_Weighted_MA(series float src, simple int length) =>
    var float[] cosine_weights = array.new_float(0)
    array.clear(cosine_weights)
    for i = 0 to length - 1
        weight = math.cos((math.pi * (i + 1)) / length) + 1  // Shift by adding 1
        array.push(cosine_weights, weight)
    sum_weights = array.sum(cosine_weights)
    for i = 0 to length - 1
        norm_weight = array.get(cosine_weights, i) / sum_weights
        array.set(cosine_weights, i, norm_weight)
    cwma = 0.0
    if bar_index >= length
        for i = 0 to length - 1
            cwma += array.get(cosine_weights, i) * src[i]
    cwma

// Function for Cosine Weighted ATR
f_Cosine_Weighted_ATR(simple int length) =>
    var float[] cosine_weights_atr = array.new_float(0)
    array.clear(cosine_weights_atr)
    for i = 0 to length - 1
        weight = math.cos((math.pi * (i + 1)) / length) + 1  // Shift by adding 1
        array.push(cosine_weights_atr, weight)
    sum_weights_atr = array.sum(cosine_weights_atr)
    for i = 0 to length - 1
        norm_weight_atr = array.get(cosine_weights_atr, i) / sum_weights_atr
        array.set(cosine_weights_atr, i, norm_weight_atr)
    cwatr = 0.0
    tr = ta.tr(true)  // True Range
    if bar_index >= length
        for i = 0 to length - 1
            cwatr += array.get(cosine_weights_atr, i) * tr[i]
    cwatr

// Use normal ATR or Cosine Weighted ATR based on input
atr = atr_type == "Normal ATR" ? ta.atr(atr_length) : f_Cosine_Weighted_ATR(atr_length)

// Calculate Cosine Weighted Moving Average
cwma = f_Cosine_Weighted_MA(price, ma_length)

// McGinley Dynamic Function
mcginley_dynamic(series float src, float fPeriod, float fK, float fExponent) =>
    period = math.max(1.0, fPeriod)
    float md = na
    prior_md = nz(md[1], src)
    md := prior_md + (src - prior_md) / math.min(period, math.max(1.0, fK * period * math.pow(src / prior_md, fExponent)))
    md

// Calculate Supertrend with Clustering Influence
perf = ta.ema(math.abs(close - close[1]), int(perf_memory))
supertrend_multiplier_adj = supertrend_multiplier * (1 - math.max(clustering_adjustment_factor, 0.5 - perf / 100))

// Original Supertrend Calculation
up = price - supertrend_multiplier_adj * atr
dn = price + supertrend_multiplier_adj * atr

// Apply McGinley Dynamic to 'up' and 'dn'
up_mcginley = mcginley_dynamic(up, mcginley_period, mcginley_k, mcginley_exponent)
dn_mcginley = mcginley_dynamic(dn, mcginley_period, mcginley_k, mcginley_exponent)

// Calculate 'up1' and 'dn1' using McGinley smoothed values
up1 = nz(up_mcginley[1], up_mcginley)
dn1 = nz(dn_mcginley[1], dn_mcginley)

// Adjust 'up' and 'dn' with McGinley smoothing
up_mcginley := price[1] > up1 ? math.max(up_mcginley, up1) : up_mcginley
dn_mcginley := price[1] < dn1 ? math.min(dn_mcginley, dn1) : dn_mcginley

// Trend Calculation using McGinley smoothed values
trend = 1
trend := nz(trend[1], trend)
trend := trend == -1 and price > dn1 ? 1 : trend == 1 and price < up1 ? -1 : trend

// Plot McGinley Smoothed Supertrend Lines
plot(trend == 1 ? up_mcginley : na, title='McGinley Up Trend', style=plot.style_linebr, linewidth=2, color=color.new(color.green, 0))
plot(trend == -1 ? dn_mcginley : na, title='McGinley Down Trend', style=plot.style_linebr, linewidth=2, color=color.new(color.red, 0))

// Calculate the midpoint between the up and down levels of the Supertrend
midpoint = (up_mcginley + dn_mcginley) / 2

// Optionally plot the midpoint as a reference line for stop-loss purposes
plot(midpoint, title="Supertrend Midpoint", color=color.blue, linewidth=2, style=plot.style_line)

// Indicator ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Long Entry/Exit
longCondition = trend == 1 and price > cwma and ta.valuewhen(trend == 1 and price > cwma, trend, 1) == 1
closeLong = trend == -1 and ta.valuewhen(trend == -1, trend, 1) == -1

// New Condition for Closing Part of Long if the Close is Below Midpoint (using full bar close)
closeLongPartial = barstate.isconfirmed and close < midpoint

// Short Entry/Exit
shortCondition = trend == -1 and price < cwma and ta.valuewhen(trend == -1 and price < cwma, trend, 1) == -1
closeShort = trend == 1 and ta.valuewhen(trend == 1, trend, 1) == 1

// New Condition for Closing Part of Short if the Close is Above Midpoint (using full bar close)
closeShortPartial = barstate.isconfirmed and close > midpoint

// Enable long / short
longEnabled = input(true)
shortEnabled = input(false)

// Bot web-link alert - {{strategy.order.comment}}
botLONG = ''
botCLOSELONG = ''
botSHORT = ''
botCLOSESHORT = ''

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// BACKTEST ENGINE \\\\\\\\\\\\\\\\\\\\\\\\\/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////// [NO USER INPUT REQUIRED] //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

// Order Execution
if testPeriod()
    if longCondition and longEnabled
        strategy.entry(id='Long', direction=strategy.long, comment=botLONG)  // Long Entry

    if closeLongPartial and longEnabled and strategy.position_size > 0
        strategy.order(id='PartialCloseLong', direction=strategy.short, qty=strategy.position_size * (partial_close_percent / 100), comment="Close Part of Long at Midpoint")  // Close Part of Long if Close < Midpoint
    
    if closeLong
        strategy.close(id='Long', comment=botCLOSELONG)  // Full Long Close

    if shortCondition and shortEnabled
        strategy.entry(id='Short', direction=strategy.short, comment=botSHORT)  // Short Entry

    if closeShortPartial and shortEnabled and strategy.position_size < 0
        strategy.order(id='PartialCloseShort', direction=strategy.long, qty=math.abs(strategy.position_size) * (partial_close_percent / 100), comment="Close Part of Short at Midpoint")  // Close Part of Short if Close > Midpoint
    
    if closeShort
        strategy.close(id='Short', comment=botCLOSESHORT)  // Full Short Close

// TP/SL Execution
if strategy.position_size > 0
    strategy.exit(id='Long SL/TP', from_entry='Long', limit=longProfitPrice, stop=longStopPrice)

if strategy.position_size < 0
    strategy.exit(id='Short TP/SL', from_entry='Short', limit=shortProfitPrice, stop=shortStopPrice)

// Draw Entry, TP and SL Levels for Long Positions
plot(strategy.position_size > 0 ? longTP == 0 ? na : longProfitPrice : na, style=plot.style_linebr, color=color.new(color.green, 0), title='Long TP')
plot(strategy.position_size > 0 ? strategy.position_avg_price : na, style=plot.style_linebr, color=color.new(color.blue, 0), title='Long Entry')
plot(strategy.position_size > 0 ? longSL == 0 ? na : longStopPrice : na, style=plot.style_linebr, color=color.new(color.red, 0), title='Long SL')
// Draw Entry, TP and SL Levels for Short Positions
plot(strategy.position_size < 0 ? shortTP == 0 ? na : shortProfitPrice : na, style=plot.style_linebr, color=color.new(color.green, 0), title='Short TP')
plot(strategy.position_size < 0 ? strategy.position_avg_price : na, style=plot.style_linebr, color=color.new(color.blue, 0), title='Short Entry')
plot(strategy.position_size < 0 ? shortSL == 0 ? na : shortStopPrice : na, style=plot.style_linebr, color=color.new(color.red, 0), title='Short SL')
