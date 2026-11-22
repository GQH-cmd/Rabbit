/*
 * Arduino CLI requires the primary .ino file to match this directory name.
 * Arduino also generates function declarations before concatenating secondary
 * .ino files, so task-link types must be visible here first.
 */
#include <Arduino.h>
#include "FocLinkProtocol.h"
#include "GatewayValidation.h"
