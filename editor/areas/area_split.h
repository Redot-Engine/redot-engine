#pragma once

#include "scene/gui/split_container.h"

class AreaSplit : public SplitContainer {
	GDCLASS(AreaSplit, SplitContainer);

public:
	AreaSplit() {
		set_dragger_visibility(DRAGGER_VISIBLE);
	}
};
