#include "area_pane.h"

#include "area_editor_node.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/button.h"

void AreaPane::_notification(int p_what) {
	// Reserved for theme updates later.
}

void AreaPane::gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		// Pass the LOCAL position so AreaEditorNode can classify the gesture:
		// press on header = move-content drag; press on body edge = split drag.
		AreaEditorNode::get_singleton()->begin_pane_press(this, mb->get_global_position(), is_header_pos(mb->get_position()));
		accept_event();
	}
}

bool AreaPane::is_header_pos(const Vector2 &p_local) const {
	return header && header->get_rect().has_point(p_local);
}

void AreaPane::set_pane_title(const String &p_title) {
	title_label->set_text(p_title);
}

void AreaPane::set_pane_content(Control *p_content) {
	if (content) {
		vbox->remove_child(content);
	}
	content = p_content;
	if (content) {
		content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		content->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		vbox->add_child(content);
	}
}


Control *AreaPane::take_pane_content() {
	Control *c = content;
	if (c) {
		vbox->remove_child(c);
		content = nullptr;
	}
	return c;
}

void AreaPane::_on_close_pressed() {
	AreaEditorNode::get_singleton()->close_pane(this);
}

AreaPane::AreaPane(const String &title) {
	set_custom_minimum_size(Size2(120, 80));

	vbox = memnew(VBoxContainer);
	vbox->add_theme_constant_override("separation", 0);
	add_child(vbox);

	header = memnew(PanelContainer);
	vbox->add_child(header);   // header must NOT be MOUSE_FILTER_IGNORE — it's the
	                         // move handle. (Your diff set it to IGNORE, which also
	                         // prevented distinguishing header vs edge presses.)

	HBoxContainer *header_hbox = memnew(HBoxContainer);
	header->add_child(header_hbox);

	title_label = memnew(Label);
	title_label->set_text(title);
	title_label->set_mouse_filter(MOUSE_FILTER_IGNORE); // label ignores so header gets the event
	title_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header_hbox->add_child(title_label);

	close_button = memnew(Button);
	close_button->set_text("X");
	close_button->set_flat(true);
	close_button->set_focus_mode(Control::FOCUS_NONE);

	// The button handles its own press--don't let it start a drag
	close_button->connect("pressed", callable_mp(this, &AreaPane::_on_close_pressed));
	header_hbox->add_child(close_button);




	set_mouse_filter(MOUSE_FILTER_STOP);
}
