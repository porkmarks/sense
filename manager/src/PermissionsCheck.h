#pragma once
#include "DB.h"

class QWidget;

extern bool adminCheck(DB& db, QWidget* parent);
extern bool hasPermission(DB& db, DB::UserDescriptor::Permissions permission);
extern bool hasPermissionOrCanLoginAsAdmin(DB& db, DB::UserDescriptor::Permissions permission, QWidget* parent);
