//@version=5
indicator("FDI and LHEA Combined with Williams Fractal", overlay=true)

length = input.int(30, title="Length")
smoothing_length = input.int(1, title="Smoothing Length")
smoothing = input.string("ZLEMA", title="Smoothing", options=["MG","RMA", "SMA", "EMA", "WMA",  "ZLEMA", "Super Smoother Filter", "2 Pole Butterworth Filter", "3 Pole Butterworth Filter", "Ehlers Hamming MA", "Ehlers Instantaneous Trendline"])
fractal_period = input.int(title="Fractal Periods", defval=9, minval=1)  // Williams Fractal Period
useSmoothing = input.bool(defval = false, title = "Use smoothing?", group = "Smoothing settings")

// Define smoothing function
smoothed_ma(source, length) =>
    if smoothing == "MG"
        mg = 0.0
        _ema = ta.ema(source, length)
        mg := na(mg[1]) ? _ema : mg[1] + (source - mg[1]) / (length * math.pow(source / mg[4], 1))
        mg
    else if smoothing == "RMA"
        ta.rma(source, length)
    else if smoothing == "SMA"
        ta.sma(source, length)
    else if smoothing == "EMA"
        ta.ema(source, length)
    else if smoothing == "WMA"
        ta.wma(source, length)
    else if smoothing == "ZLEMA"
        ta.ema(source + (source - source[length]), length)
    else if smoothing == "Super Smoother Filter"
        a1 = math.exp(-1.414 * 3.14159 / length)
        b1 = 2 * a1 * math.cos(1.414 * 3.14159 / length)
        c2 = b1
        c3 = -a1 * a1
        c1 = 1 - c2 - c3
        var float super_smoother_value = na  // Initialize the super smoother value
        super_smoother_value := na(super_smoother_value[1]) ? source : c1 * (source + nz(source[1])) / 2 + c2 * nz(super_smoother_value[1]) + c3 * nz(super_smoother_value[2])
    else if smoothing == "2 Pole Butterworth Filter"
        PI = 2 * math.asin(1)
        a = math.exp(-math.sqrt(2) * PI / length)
        b = 2 * a * math.cos(math.sqrt(2) * PI / length)
        bf = 0.0
        bf := b * nz(bf[1]) - math.pow(a, 2) * nz(bf[2]) + (1 - b + math.pow(a, 2)) / 4 * (source + 2 * nz(source[1]) + nz(source[2]))
        bf
    else if smoothing == "3 Pole Butterworth Filter"
        PI = 2 * math.asin(1)
        a = math.exp(-PI / length)
        b = 2 * a * math.cos(1.738 * PI / length)
        c = math.pow(a, 2)
        bf = 0.0
        bf := (b + c) * nz(bf[1]) - (c + b * c) * nz(bf[2]) + math.pow(c, 2) * nz(bf[3]) + (1 - b + c) * (1 - c) / 8 * (source + 3 * nz(source[1]) + 3 * nz(source[2]) + nz(source[3]))
        bf
    else if smoothing == "Ehlers Hamming MA"
        pedestal = 3.0  // Fixed pedestal value
        filt = 0.0
        coef = 0.0
        for i = 0 to length - 1
            sine = math.sin(pedestal + ((math.pi - (2 * pedestal)) * i / (length - 1)))
            filt := filt + (sine * nz(source[i]))
            coef := coef + sine
        filt := coef != 0 ? filt / coef : 0
        filt
    else if smoothing == "Ehlers Instantaneous Trendline"
        var float itrend = na  // Initialize itrend
        alpha = 2.0 / (length + 1)
        if (bar_index < 7)
            itrend := (source + 2 * nz(source[1]) + nz(source[2])) / 4
        else
            itrend := (alpha - math.pow(alpha, 2) / 4) * source 
            itrend += 0.5 * math.pow(alpha, 2) * nz(source[1]) 
            itrend -= (alpha - 0.75 * math.pow(alpha, 2)) * nz(source[2]) 
            itrend += 2 * (1 - alpha) * nz(itrend[1]) 
            itrend -= math.pow(1 - alpha, 2) * nz(itrend[2])
        itrend

// LHEA Calculation
_LHEA(length) =>
    atr = smoothed_ma(ta.tr(true), length)
    hh = ta.highest(high, length)
    ll = ta.lowest(low, length)
    H = (math.log(hh - ll) - math.log(atr)) / math.log(length)
    H

lhea_raw = _LHEA(length)

// FDI Calculation
_FDI(length) =>
    hh = ta.highest(close, length)
    ll = ta.lowest(close, length)
    cumulative_length = 0.0
    for i = 1 to length - 1
        diff = (close[i] - ll) / (hh - ll)
        diff_next = (close[i + 1] - ll) / (hh - ll)
        cumulative_length := cumulative_length + math.sqrt(math.pow(diff - diff_next, 2) + (1 / math.pow(length, 2)))
    1 + (math.log(cumulative_length) + math.log(2)) / math.log(2 * length)

fdi_raw = _FDI(length)

// Normalize and Invert FDI to a 0-1 scale
fdi_normalized_inverted = 1 - ((fdi_raw - 1) / (2 - 1))

// Apply smoothing if selected
fdi_input = useSmoothing ? smoothed_ma(fdi_normalized_inverted, smoothing_length) : fdi_normalized_inverted
lhea_input = useSmoothing ? smoothed_ma(lhea_raw, smoothing_length) : lhea_raw

// Define a Williams Fractal function
williams_fractal(source, period) =>
    bool upFractal = true
    bool downFractal = true
    for i = 1 to period
        upFractal := upFractal and (source[period - i] < source[period])
        downFractal := downFractal and (source[period - i] > source[period])
    [upFractal and source[period] > source[period + 1], downFractal and source[period] < source[period + 1]]

// Get fractal conditions for FDI and LHEA using the function
[up_fractal_fdi, down_fractal_fdi] = williams_fractal(fdi_input, fractal_period)
[up_fractal_lhea, down_fractal_lhea] = williams_fractal(lhea_input, fractal_period)

// Plot fractals for FDI and LHEA
plotshape(up_fractal_fdi, style=shape.triangleup, location=location.abovebar, color=color.blue, size=size.small, title="FDI Up Fractal")
plotshape(down_fractal_fdi, style=shape.triangledown, location=location.belowbar, color=color.blue, size=size.small, title="FDI Down Fractal")
plotshape(up_fractal_lhea, style=shape.triangleup, location=location.abovebar, color=color.orange, size=size.small, title="LHEA Up Fractal")
plotshape(down_fractal_lhea, style=shape.triangledown, location=location.belowbar, color=color.orange, size=size.small, title="LHEA Down Fractal")
