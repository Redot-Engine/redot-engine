#pragma once

#include "scene/gui/panel_container.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"


class Label;
class Button;

// A leaf of the layout tree: a header (grab handle + title) plus content.
class AreaPane : public PanelContainer {
	GDCLASS(AreaPane, PanelContainer);

	Label *title_label = nullptr;
	PanelContainer *header = nullptr;
	Control *content = nullptr;   // swappable; the pane itself is never reparented by a drop
	VBoxContainer *vbox = nullptr;

	Button *close_button = nullptr; ///< Closes the pane

protected:
	void _notification(int p_what);
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	void _on_close_pressed();

public:
	void set_pane_title(const String &p_title);
	String get_pane_title() const { return title_label->get_text(); }
	/// Remove the content (Control) from the pane, then add the dragged one in, set it to expand, etc...
	void set_pane_content(Control *p_content);
	Control *get_pane_content() const { return content; }
	/// Move-gesture support: caller takes ownership; pane becomes "empty"
	/// (later: show the Blender-style type switcher here).
	Control *take_pane_content();          // detaches content without deleting; for move-drops
	bool is_header_pos(const Vector2 &p_local) const; // header vs body hit-test

	AreaPane(const String &title = "Pane");
};
