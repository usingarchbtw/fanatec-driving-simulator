function varargout = fanatec_sfunction(varargin)
% FANATEC_SFUNCTION - Professional Fanatec Pedal Interface for dSPACE
%
% STANDALONE S-function that directly reads Fanatec pedals via Raw Input HID
% No dependencies - uses same proven logic as working GUI application
%
% Output Ports (5 signals):
%  1: Vehicle Speed    (0-1 normalized, 0-300 km/h)  -> UWORD in A2L
%  2: Drive Mode       (0=Static, 1=Dynamic)         -> UBYTE in A2L  
%  3: Throttle Raw     (0-1 normalized, 0-255)       -> UBYTE in A2L
%  4: Brake Raw        (0-1 normalized, 0-255)       -> UBYTE in A2L
%  5: Clutch State     (0=Released, 1=Pressed)       -> UBYTE in A2L
%
% XCP/dSPACE Integration:
%  - Variables match MEASUREMENT blocks in your A2L file
%  - 100Hz sample rate for real-time performance
%  - Direct Raw Input HID (no external dependencies)
%

switch varargin{1}
    case 0
        [sys,x0,str,ts] = mdlInitializeSizes;
        varargout = {sys,x0,str,ts};
    case 1
        sys = mdlDerivatives(varargin{2:4});
        varargout = {sys};
    case 2
        sys = mdlUpdate(varargin{2:4});
        varargout = {sys};
    case 3
        mdlOutputs(varargin{2:4});
        varargout = {[]};
    case 9
        mdlTerminate(varargin{2:4});
        varargout = {[]};
    otherwise
        if nargout > 0
            varargout = cell(1,nargout);
        end
end
end

function [sys,x0,str,ts] = mdlInitializeSizes
sizes = simsizes;
sizes.NumContStates  = 0;
sizes.NumDiscStates  = 0;
sizes.NumOutputs     = 5;  % [speed, mode, throttle, brake, clutch]
sizes.NumInputs      = 0;  % Standalone - no inputs
sizes.DirFeedthrough = 0;
sizes.NumSampleTimes = 1;
sys = simsizes(sizes);
x0  = [];
str = [];
ts  = [0.01 0]; % 100Hz sample time
end