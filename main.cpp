#include <elements.hpp>
#include <infra/support.hpp>
#include <thread>
#include <memory>

#include "Serial.hpp"

using namespace std::chrono_literals;

using namespace cycfi;
using namespace cycfi::elements;

constexpr float lower_bound_serial = 0.0f;
constexpr float upper_bound_serial = 1023.0f;

static float mapping(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

class plotter : public tracker<>, public receiver<float>
{
public:
	void draw(context const& ctx) override
	{
		// fixes where points starts to be drawn
		float repair = 3.6f;

		// region where can draw
		auto bounds = rect(ctx.bounds.left + repair, ctx.bounds.top + repair,
			ctx.bounds.right - repair, ctx.bounds.bottom - repair);

		// red circle color
		ctx.canvas.fill_style(color(1, 0, 0));

		auto total_seconds = 30;
		auto fsample = 1.0; // sample rate = 1hz
		auto Tsample = 1.0 / fsample; // sec
		auto num_points = total_seconds / Tsample; // number dots drew

		auto dx = bounds.width() / num_points; // increment between each dot

		float posx = bounds.bottom_left().x;

		for (auto amp : amplitude)
		{
			// correct amplitude y position
			float posy = mapping(amp, lower_bound_serial, upper_bound_serial,
				bounds.top_left().y, bounds.bottom_left().y);

			// draw circles
			ctx.canvas.circle(circle({ posx, posy }, 3));
			ctx.canvas.fill();

			posx += dx;
			if (posx > bounds.bottom_right().x) {
				posx = bounds.bottom_left().x;
				amplitude.clear();
			}
		}
	}

	// receiver api
	// Receiver API
	float       value() const override { return last_value; }
	void        value(float val) override
	{
		last_value = val;
		amplitude.emplace_back(val);
	}

	using on_change_f = std::function<void(float)>;
	on_change_f on_change;

	std::vector<float> amplitude;

private:
	float last_value{ 60 };
};

class my_app : public app
{
public:
	my_app(int argc, char* argv[]);

	void set_value(float y)
	{
		_control.value(y);
		_view.refresh();
	}

	bool paused{ true };

private:
	window _win;
	view _view;
	plotter _control;

	auto make_buttons()
	{
		auto tbutton = toggle_button("On", 1.0, colors::red.opacity(0.4));
		tbutton.on_click = [this](bool b) { paused = !paused; };
		return margin({ 20, 10, 20, 10 }, vtile(align_center(tbutton)));
	}

	auto make_signal()
	{
		return
			align_center_middle(
				layer(
					vgrid_lines(10, 10),
					hgrid_lines(10, 10),
					link(_control) // draw
				)
			);
	}

	auto make_control_panel()
	{
		return margin({ 20, 20, 20, 20 }, pane("Control", make_buttons(), 0.8f));
	}

	auto make_signal_panel()
	{
		return margin({ 20, 20, 20, 20 }, pane("Signal", make_signal()));
	}
};

my_app::my_app(int argc, char* argv[])
	: app(argc, argv, "Serial Plot", "plot_serial_id")
	, _win{ name() }
	, _view{ _win }
{
	_win.on_close = [this]() { stop();  };

	_view.content(
		margin({ 20, 10, 10, 20 },
			vmin_size(300,
				hmin_size(500,
					vtile(
						make_signal_panel(),
						make_control_panel()
					)
				)
			)),

		// background
		box(rgba(35, 35, 37, 255))
	);
}

void serial_values(std::stop_token stoken, std::shared_ptr<my_app> app)
{
	//start
	light::serial serial("COM5");
	auto msg = serial.begin(9600);
	while (msg != light::message::successful_begin)
	{
		if (stoken.stop_requested())
			break;

		std::this_thread::sleep_for(1000ms);
		msg = serial.begin(9600);
	}

	//request values
	while (!stoken.stop_requested())
	{
		if (app->paused)
			continue;

		std::string text = serial.read();
		float value = std::stof(text);

		app->set_value(value);

		std::this_thread::sleep_for(1000ms);
	}
}

int main(int argc, char* argv[])
{
	auto app = std::make_shared<my_app>(argc, argv);

	std::jthread jth_serial(serial_values, app);

	app->run();

	jth_serial.request_stop();

	return 0;
}