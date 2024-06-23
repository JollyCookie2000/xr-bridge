// RAII
// TODO: Allow using macros to choose the error handling method (exceptions vs is_ready()).

class XrBridge
{
public:
	XrBridge(const std::string& appication_name);
	~XrBridge();

	bool is_ready() const;
private:
	bool is_ready_flag;
};

XrBridge::XrBridge(const std::string& application_name)
{
	this->is_ready_flag = false;

	// Check if application_name is too long

	/* Allocate resources */

	// Throw an exception on error

	this->is_ready_flag = true;
}

XrBridge::~XrBridge()
{
	/* De-allocate resources */

	// Never throw
}

bool XrBridge::is_ready() const
{
	return this->is_ready_flag;
}
