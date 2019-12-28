#pragma once
#include "Settings.h"

class QWidget;

extern bool adminCheck(Settings& settings, QWidget* parent);
extern bool hasPermission(Settings& settings, Settings::UserDescriptor::Permissions permission);
extern bool hasPermissionOrCanLoginAsAdmin(Settings& settings, Settings::UserDescriptor::Permissions permission, QWidget* parent);
