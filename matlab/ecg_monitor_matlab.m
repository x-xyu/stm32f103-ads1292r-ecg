function ecg_monitor_matlab(portName)
%ECG_MONITOR_MATLAB Real-time raw/filtered ECG display and heart-rate estimate.
%   ecg_monitor_matlab("COM10")
%
% No additional MATLAB toolbox is required.  The firmware samples the ADC
% at 250 SPS and transmits every second sample, giving a 125-SPS stream.

if nargin < 1
    portName = "COM10";
end

baudRate = 115200;
adcIndexRate = 250;
streamRate = 125;
visibleSeconds = 10;
maximumPoints = streamRate * visibleSeconds;

device = serialport(portName, baudRate, "Timeout", 1);
cleanupDevice = onCleanup(@() delete(device));
configureTerminator(device, "LF");
flush(device);

figureHandle = figure( ...
    "Name", "ADS1292R ECG监视器", ...
    "NumberTitle", "off", ...
    "Color", "white");
layout = tiledlayout(figureHandle, 2, 1, ...
    "TileSpacing", "compact", "Padding", "compact");

rawAxes = nexttile(layout, 1);
rawLine = animatedline(rawAxes, ...
    "Color", [0.45, 0.65, 0.85], ...
    "LineWidth", 0.8, ...
    "MaximumNumPoints", maximumPoints);
grid(rawAxes, "on");
ylabel(rawAxes, "原始ADC值");
title(rawAxes, "原始心电信号");

filteredAxes = nexttile(layout, 2);
filteredLine = animatedline(filteredAxes, ...
    "Color", [0.00, 0.41, 0.71], ...
    "LineWidth", 1.2, ...
    "MaximumNumPoints", maximumPoints);
grid(filteredAxes, "on");
xlabel(filteredAxes, "时间 / s");
ylabel(filteredAxes, "滤波后幅值");
title(filteredAxes, "0.5–35 Hz带通 + 50 Hz陷波");
bpmText = text(filteredAxes, 0.02, 0.90, "心率：等待数据", ...
    "Units", "normalized", ...
    "FontSize", 13, ...
    "FontWeight", "bold", ...
    "Color", [0.80, 0.15, 0.12]);

% First-order 0.5-Hz high-pass and 35-Hz low-pass filters.
samplePeriod = 1 / streamRate;
highPassRC = 1 / (2 * pi * 0.5);
highPassAlpha = highPassRC / (highPassRC + samplePeriod);
lowPassRC = 1 / (2 * pi * 35);
lowPassAlpha = samplePeriod / (lowPassRC + samplePeriod);

% 50-Hz notch biquad, Q=25.  Coefficients use a0-normalized form.
notchOmega = 2 * pi * 50 / streamRate;
notchAlpha = sin(notchOmega) / (2 * 25);
notchA0 = 1 + notchAlpha;
notchB0 = 1 / notchA0;
notchB1 = -2 * cos(notchOmega) / notchA0;
notchB2 = 1 / notchA0;
notchA1 = -2 * cos(notchOmega) / notchA0;
notchA2 = (1 - notchAlpha) / notchA0;

firstIndex = [];
lastIndex = [];
lastAcceptedValue = [];
sampleCount = 0;

notchX1 = 0;
notchX2 = 0;
notchY1 = 0;
notchY2 = 0;
highPassInputPrevious = 0;
highPassOutputPrevious = 0;
lowPassOutputPrevious = 0;

heartRateTimes = zeros(1, 0);
heartRateValues = zeros(1, 0);
lastHeartRateUpdate = -inf;

fprintf("已打开 %s，波特率 %d。关闭图窗即可退出。\n", portName, baudRate);

while isgraphics(figureHandle)
    if device.NumBytesAvailable == 0
        drawnow limitrate;
        pause(0.002);
        continue;
    end

    textLine = strtrim(readline(device));
    tokens = regexp(char(textLine), ...
        '^\s*(-?\d+)\s*,\s*(-?\d+)\s*$', "tokens", "once");

    if isempty(tokens)
        if strlength(textLine) > 0
            fprintf("%s\n", textLine);
        end
        continue;
    end

    sampleIndex = str2double(tokens{1});
    rawValue = str2double(tokens{2});

    if ~isempty(lastIndex) && sampleIndex < lastIndex
        clearpoints(rawLine);
        clearpoints(filteredLine);
        firstIndex = [];
        lastAcceptedValue = [];
        heartRateTimes = zeros(1, 0);
        heartRateValues = zeros(1, 0);
        notchX1 = 0; notchX2 = 0; notchY1 = 0; notchY2 = 0;
        highPassInputPrevious = 0;
        highPassOutputPrevious = 0;
        lowPassOutputPrevious = 0;
    end

    if rawValue < -8388608 || rawValue > 8388607
        continue;
    end

    % Reject only extreme one-sample digital-transfer glitches.  ECG and
    % ordinary motion artefacts remain visible for honest signal inspection.
    if ~isempty(lastAcceptedValue) && abs(rawValue - lastAcceptedValue) > 1000000
        fprintf("丢弃数字传输异常点：%.0f\n", rawValue);
        continue;
    end

    if isempty(firstIndex)
        firstIndex = sampleIndex;
        highPassInputPrevious = rawValue;
    end
    lastIndex = sampleIndex;
    lastAcceptedValue = rawValue;
    timeSeconds = (sampleIndex - firstIndex) / adcIndexRate;

    % 50-Hz notch.
    notchValue = notchB0 * rawValue + notchB1 * notchX1 + notchB2 * notchX2 ...
        - notchA1 * notchY1 - notchA2 * notchY2;
    notchX2 = notchX1;
    notchX1 = rawValue;
    notchY2 = notchY1;
    notchY1 = notchValue;

    % Baseline removal followed by low-pass noise reduction.
    highPassValue = highPassAlpha * (highPassOutputPrevious + ...
        notchValue - highPassInputPrevious);
    highPassInputPrevious = notchValue;
    highPassOutputPrevious = highPassValue;
    filteredValue = lowPassOutputPrevious + ...
        lowPassAlpha * (highPassValue - lowPassOutputPrevious);
    lowPassOutputPrevious = filteredValue;

    addpoints(rawLine, timeSeconds, rawValue);
    addpoints(filteredLine, timeSeconds, filteredValue);
    sampleCount = sampleCount + 1;

    heartRateTimes(end + 1) = timeSeconds; %#ok<AGROW>
    heartRateValues(end + 1) = filteredValue; %#ok<AGROW>
    keep = heartRateTimes >= (timeSeconds - 12);
    heartRateTimes = heartRateTimes(keep);
    heartRateValues = heartRateValues(keep);

    if timeSeconds - lastHeartRateUpdate >= 0.5
        bpm = estimateHeartRate(heartRateTimes, heartRateValues);
        if isnan(bpm)
            bpmText.String = "心率：等待稳定波形";
        else
            bpmText.String = sprintf("心率：%.0f bpm（实验估算）", bpm);
        end
        lastHeartRateUpdate = timeSeconds;
    end

    if mod(sampleCount, 10) == 0
        xLeft = max(0, timeSeconds - visibleSeconds);
        xRight = max(visibleSeconds, timeSeconds);
        xlim(rawAxes, [xLeft, xRight]);
        xlim(filteredAxes, [xLeft, xRight]);
        ylim(rawAxes, "auto");
        ylim(filteredAxes, "auto");
        drawnow limitrate;
    end
end
end


function bpm = estimateHeartRate(times, signal)
% Robust, toolbox-free QRS interval estimate from the latest data window.
bpm = NaN;
if numel(signal) < 375
    return;
end

envelope = abs(signal);
centre = median(envelope);
deviation = median(abs(envelope - centre));
threshold = centre + 4 * max(deviation, 1);

candidate = find( ...
    envelope(2:end-1) > envelope(1:end-2) & ...
    envelope(2:end-1) >= envelope(3:end) & ...
    envelope(2:end-1) > threshold) + 1;

if isempty(candidate)
    return;
end

% Keep only the strongest peak inside each 300-ms refractory interval.
selected = candidate(1);
for k = 2:numel(candidate)
    current = candidate(k);
    previous = selected(end);
    if times(current) - times(previous) >= 0.30
        selected(end + 1) = current; %#ok<AGROW>
    elseif envelope(current) > envelope(previous)
        selected(end) = current;
    end
end

if numel(selected) < 4
    return;
end

intervals = diff(times(selected));
intervals = intervals(intervals >= 0.35 & intervals <= 1.50);
if numel(intervals) < 3
    return;
end

bpm = 60 / median(intervals);
if bpm < 40 || bpm > 180
    bpm = NaN;
end
end
