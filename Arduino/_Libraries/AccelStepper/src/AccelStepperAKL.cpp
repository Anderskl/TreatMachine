// AccelStepperAKL.cpp
//
// Copyright (C) 2009-2020 Mike McCauley
// $Id: AccelStepperAKL.cpp,v 1.24 2020/04/20 00:15:03 mikem Exp mikem $

#include "AccelStepperAKL.h"

#if 0
// Some debugging assistance
void dump(uint8_t* p, int l)
{
    int i;

    for (i = 0; i < l; i++)
    {
	Serial.print(p[i], HEX);
	Serial.print(" ");
    }
    Serial.println("");
}
#endif

void AccelStepperAKL::moveTo(long absolute)
{
    if (_targetPos != absolute)
    {
	_targetPos = absolute;
	computeNewSpeed();
	// compute new n?
    }
}

void AccelStepperAKL::move(long relative)
{
    moveTo(_currentPos + relative);
}

// Implements steps according to the current step interval
// You must call this at least once per step
// returns true if a step occurred
boolean AccelStepperAKL::runSpeed()
{
    // Dont do anything unless we actually have a step interval
    if (!_stepInterval)
	return false;

    unsigned long time = micros();   
    if (time - _lastStepTime >= _stepInterval)
    {
	if (_direction == DIRECTION_CW)
	{
	    // Clockwise
	    _currentPos += 1;
	}
	else
	{
	    // Anticlockwise  
	    _currentPos -= 1;
	}
	step(_currentPos);

	_lastStepTime = time; // Caution: does not account for costs in step()

	return true;
    }
    else
    {
	return false;
    }
}

long AccelStepperAKL::distanceToGo()
{
    return _targetPos - _currentPos;
}

long AccelStepperAKL::targetPosition()
{
    return _targetPos;
}

long AccelStepperAKL::currentPosition()
{
    return _currentPos;
}

// Useful during initialisations or after initial positioning
// Sets speed to 0
void AccelStepperAKL::setCurrentPosition(long position)
{
    _targetPos = _currentPos = position;
    _n = 0;
    _stepInterval = 0;
    _speed = 0.0;
}

// Subclasses can override
unsigned long AccelStepperAKL::computeNewSpeed()
{
    long distanceTo = distanceToGo(); // +ve is clockwise from curent location

    long stepsToStop = (long)((_speed * _speed) / (2.0 * _acceleration)); // Equation 16

    if (distanceTo == 0 && stepsToStop <= 1)
    {
	// We are at the target and its time to stop
	_stepInterval = 0;
	_speed = 0.0;
	_n = 0;
	return _stepInterval;
    }

    if (distanceTo > 0)
    {
	// We are anticlockwise from the target
	// Need to go clockwise from here, maybe decelerate now
	if (_n > 0)
	{
	    // Currently accelerating, need to decel now? Or maybe going the wrong way?
	    if ((stepsToStop >= distanceTo) || _direction == DIRECTION_CCW)
		_n = -stepsToStop; // Start deceleration
	}
	else if (_n < 0)
	{
	    // Currently decelerating, need to accel again?
	    if ((stepsToStop < distanceTo) && _direction == DIRECTION_CW)
		_n = -_n; // Start accceleration
	}
    }
    else if (distanceTo < 0)
    {
	// We are clockwise from the target
	// Need to go anticlockwise from here, maybe decelerate
	if (_n > 0)
	{
	    // Currently accelerating, need to decel now? Or maybe going the wrong way?
	    if ((stepsToStop >= -distanceTo) || _direction == DIRECTION_CW)
		_n = -stepsToStop; // Start deceleration
	}
	else if (_n < 0)
	{
	    // Currently decelerating, need to accel again?
	    if ((stepsToStop < -distanceTo) && _direction == DIRECTION_CCW)
		_n = -_n; // Start accceleration
	}
    }

    // Need to accelerate or decelerate
    if (_n == 0)
    {
	// First step from stopped
	_cn = _c0;
	_direction = (distanceTo > 0) ? DIRECTION_CW : DIRECTION_CCW;
    }
    else
    {
	// Subsequent step. Works for accel (n is +_ve) and decel (n is -ve).
	_cn = _cn - ((2.0 * _cn) / ((4.0 * _n) + 1)); // Equation 13
	_cn = max(_cn, _cmin); 
    }
    _n++;
    _stepInterval = _cn;
    _speed = 1000000.0 / _cn;
    if (_direction == DIRECTION_CCW)
	_speed = -_speed;

#if 0
    Serial.println(_speed);
    Serial.println(_acceleration);
    Serial.println(_cn);
    Serial.println(_c0);
    Serial.println(_n);
    Serial.println(_stepInterval);
    Serial.println(distanceTo);
    Serial.println(stepsToStop);
    Serial.println("-----");
#endif
    return _stepInterval;
}

// Run the motor to implement speed and acceleration in order to proceed to the target position
// You must call this at least once per step, preferably in your main loop
// If the motor is in the desired position, the cost is very small
// returns true if the motor is still running to the target position.
boolean AccelStepperAKL::run()
{
    if (runSpeed())
	computeNewSpeed();
    return _speed != 0.0 || distanceToGo() != 0;
}

AccelStepperAKL::AccelStepperAKL(uint8_t interface, uint8_t enablePin, uint8_t pin1, uint8_t pin2, uint8_t pin3, uint8_t pin4, bool enable)
{
    _interface = interface;
    _currentPos = 0;
    _targetPos = 0;
    _speed = 0.0;
    _maxSpeed = 0.0;
    _acceleration = 0.0;
    _sqrt_twoa = 1.0;
    _stepInterval = 0;
    _minPulseWidth = 1;
    // _enablePin = 0xff;
	_enablePin = enablePin;
    _lastStepTime = 0;
    _pin[0] = pin1;
    _pin[1] = pin2;
    _pin[2] = pin3;
    _pin[3] = pin4;
    _enableInverted = true;
    
    // NEW
    _n = 0;
    _c0 = 0.0;
    _cn = 0.0;
    _cmin = 1.0;
    _direction = DIRECTION_CCW;

    int i;
    for (i = 0; i < 4; i++)
	_pinInverted[i] = 0;
    if (enable)
	enableOutputs();
    // Some reasonable default
    setAcceleration(1);
    setMaxSpeed(1);
}

AccelStepperAKL::AccelStepperAKL(void (*forward)(), void (*backward)())
{
    _interface = 0;
    _currentPos = 0;
    _targetPos = 0;
    _speed = 0.0;
    _maxSpeed = 0.0;
    _acceleration = 0.0;
    _sqrt_twoa = 1.0;
    _stepInterval = 0;
    _minPulseWidth = 1;
    _enablePin = 6;
    _lastStepTime = 0;
    _pin[0] = 0;
    _pin[1] = 0;
    _pin[2] = 0;
    _pin[3] = 0;
    _forward = forward;
    _backward = backward;

    // NEW
    _n = 0;
    _c0 = 0.0;
    _cn = 0.0;
    _cmin = 1.0;
    _direction = DIRECTION_CCW;

    int i;
    for (i = 0; i < 4; i++)
	_pinInverted[i] = 0;
    // Some reasonable default
    setAcceleration(1);
    setMaxSpeed(1);
}

void AccelStepperAKL::setMaxSpeed(float speed)
{
    if (speed < 0.0)
       speed = -speed;
    if (_maxSpeed != speed)
    {
	_maxSpeed = speed;
	_cmin = 1000000.0 / speed;
	// Recompute _n from current speed and adjust speed if accelerating or cruising
	if (_n > 0)
	{
	    _n = (long)((_speed * _speed) / (2.0 * _acceleration)); // Equation 16
	    computeNewSpeed();
	}
    }
}

float   AccelStepperAKL::maxSpeed()
{
    return _maxSpeed;
}

void AccelStepperAKL::setAcceleration(float acceleration)
{
    if (acceleration == 0.0)
	return;
    if (acceleration < 0.0)
      acceleration = -acceleration;
    if (_acceleration != acceleration)
    {
	// Recompute _n per Equation 17
	_n = _n * (_acceleration / acceleration);
	// New c0 per Equation 7, with correction per Equation 15
	_c0 = 0.676 * sqrt(2.0 / acceleration) * 1000000.0; // Equation 15
	_acceleration = acceleration;
	computeNewSpeed();
    }
}

float   AccelStepperAKL::acceleration()
{
    return _acceleration;
}

void AccelStepperAKL::setSpeed(float speed)
{
    if (speed == _speed)
        return;
    speed = constrain(speed, -_maxSpeed, _maxSpeed);
    if (speed == 0.0)
	_stepInterval = 0;
    else
    {
	_stepInterval = fabs(1000000.0 / speed);
	_direction = (speed > 0.0) ? DIRECTION_CW : DIRECTION_CCW;
    }
    _speed = speed;
}

float AccelStepperAKL::speed()
{
    return _speed;
}

// Subclasses can override
void AccelStepperAKL::step(long step)
{
    switch (_interface)
    {
        case FUNCTION:
            step0(step);
            break;

	case DRIVER:
	    step1(step);
	    break;
    
	// case FULL2WIRE:
	    // step2(step);
	    // break;
    
	// case FULL3WIRE:
	    // step3(step);
	    // break;  

	// case FULL4WIRE:
	    // step4(step);
	    // break;  

	// case HALF3WIRE:
	    // step6(step);
	    // break;  
		
	// case HALF4WIRE:
	    // step8(step);
	    // break;  
    }
}

long AccelStepperAKL::stepForward()
{
    // Clockwise
    _currentPos += 1;
	step(_currentPos);
	_lastStepTime = micros();
    return _currentPos;
}

long AccelStepperAKL::stepBackward()
{
    // Counter-clockwise
    _currentPos -= 1;
	step(_currentPos);
	_lastStepTime = micros();
    return _currentPos;
}

// You might want to override this to implement eg serial output
// bit 0 of the mask corresponds to _pin[0]
// bit 1 of the mask corresponds to _pin[1]
// ....
void AccelStepperAKL::setOutputPins(uint8_t mask)
{
    uint8_t numpins = 2;
    if (_interface == FULL4WIRE || _interface == HALF4WIRE)
	numpins = 4;
    else if (_interface == FULL3WIRE || _interface == HALF3WIRE)
	numpins = 3;
    uint8_t i;
    for (i = 0; i < numpins; i++)
	digitalWrite(_pin[i], (mask & (1 << i)) ? (HIGH ^ _pinInverted[i]) : (LOW ^ _pinInverted[i]));
}

// 0 pin step function (ie for functional usage)
void AccelStepperAKL::step0(long step)
{
    (void)(step); // Unused
    if (_speed > 0)
	_forward();
    else
	_backward();
}

// 1 pin step function (ie for stepper drivers)
// This is passed the current step number (0 to 7)
// Subclasses can override
void AccelStepperAKL::step1(long step)
{
    (void)(step); // Unused

    // _pin[0] is step, _pin[1] is direction
    setOutputPins(_direction ? 0b10 : 0b00); // Set direction first else get rogue pulses
    setOutputPins(_direction ? 0b11 : 0b01); // step HIGH
    // Caution 200ns setup time 
    // Delay the minimum allowed pulse width
    delayMicroseconds(_minPulseWidth);
    setOutputPins(_direction ? 0b10 : 0b00); // step LOW
}

    
// Prevents power consumption on the outputs
void    AccelStepperAKL::disableOutputs()
{   
    if (! _interface) return;

    setOutputPins(0); // Handles inversion automatically
    if (_enablePin != 0xff)
    {
        pinMode(_enablePin, OUTPUT);
        digitalWrite(_enablePin, LOW ^ _enableInverted);
		// digitalWrite(_enablePin, HIGH ^ _enableInverted);
    }
}

void    AccelStepperAKL::enableOutputs()
{
    if (! _interface) 
	return;

    pinMode(_pin[0], OUTPUT);
    pinMode(_pin[1], OUTPUT);
    if (_interface == FULL4WIRE || _interface == HALF4WIRE)
    {
        pinMode(_pin[2], OUTPUT);
        pinMode(_pin[3], OUTPUT);
    }
    else if (_interface == FULL3WIRE || _interface == HALF3WIRE)
    {
        pinMode(_pin[2], OUTPUT);
    }

    if (_enablePin != 0xff)
    {
        pinMode(_enablePin, OUTPUT);
        digitalWrite(_enablePin, HIGH ^ _enableInverted);
		// digitalWrite(_enablePin, LOW ^ _enableInverted);
    }
}

void AccelStepperAKL::setMinPulseWidth(unsigned int minWidth)
{
    _minPulseWidth = minWidth;
}

void AccelStepperAKL::setEnablePin(uint8_t enablePin)
{
    _enablePin = enablePin;

    // This happens after construction, so init pin now.
    if (_enablePin != 0xff)
    {
        pinMode(_enablePin, OUTPUT);
        digitalWrite(_enablePin, HIGH ^ _enableInverted);
		// digitalWrite(_enablePin, LOW ^ _enableInverted);
    }
}

void AccelStepperAKL::setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert)
{
    _pinInverted[0] = stepInvert;
    _pinInverted[1] = directionInvert;
    _enableInverted = enableInvert;
}

void AccelStepperAKL::setPinsInverted(bool pin1Invert, bool pin2Invert, bool pin3Invert, bool pin4Invert, bool enableInvert)
{    
    _pinInverted[0] = pin1Invert;
    _pinInverted[1] = pin2Invert;
    _pinInverted[2] = pin3Invert;
    _pinInverted[3] = pin4Invert;
    _enableInverted = enableInvert;
}

// Blocks until the target position is reached and stopped
void AccelStepperAKL::runToPosition()
{
    while (run())
	YIELD; // Let system housekeeping occur
}

boolean AccelStepperAKL::runSpeedToPosition()
{
    if (_targetPos == _currentPos)
	return false;
    if (_targetPos >_currentPos)
	_direction = DIRECTION_CW;
    else
	_direction = DIRECTION_CCW;
    return runSpeed();
}

// Blocks until the new target position is reached
void AccelStepperAKL::runToNewPosition(long position)
{
    moveTo(position);
    runToPosition();
}

void AccelStepperAKL::stop()
{
    if (_speed != 0.0)
    {    
	long stepsToStop = (long)((_speed * _speed) / (2.0 * _acceleration)) + 1; // Equation 16 (+integer rounding)
	if (_speed > 0)
	    move(stepsToStop);
	else
	    move(-stepsToStop);
    }
}

bool AccelStepperAKL::isRunning()
{
    return !(_speed == 0.0 && _targetPos == _currentPos);
}
