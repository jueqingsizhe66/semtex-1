#ifndef __FILE_PARSER_HPP__
#define __FILE_PARSER_HPP__

class Context;

//! Contains the location of where to insert a replacement, and where to put it
struct Replacement {
	const char* start = nullptr; //!< Where to start the replacement
	const char* end = nullptr; //!< One byte past the end of the string to be replaced
	std::string replaceWith; //!< Immutable replacement string

	Replacement(const char* s, const char* e, std::string&& r)
		: start(s), end(e), replaceWith(std::forward<std::string>(r))
	{ }

	Replacement(const char* s, const char* e, const std::string& r)
		: start(s), end(e), replaceWith(r)
	{ }

	// Satisfies Effective C++ guidelines of providing copy and assignment for objects with pointers
	Replacement& operator=(const Replacement&) = default;
	Replacement(Replacement&) = default;
	Replacement(Replacement&&) = default;
};

//! Returned from parseArgs
struct MacroOptions {
	std::unordered_set<std::string> flags;
	std::unordered_map<std::string, std::string> opts;

	// This has a constructor, as it is not a POD type
	MacroOptions() : flags(), opts() { }
};

struct Parser {

public:
	// No need for encapsulation since nearly everything that interacts with Parser modifies these members
	//! \todo Would a linked list run faster?
	std::vector<Replacement> replacements;
	const char* const end;
	const char* curr;

	Parser(const std::string& file, const char* current, const char* end, Context& context, int startingLine = 1)
		: replacements(), end(end), curr(current), filename(file), currLine(startingLine),
		  unixNewlines(0), windowsNewlines(0), macNewlines(0), ctxt(context)
	{ }

	/*!
	 * \brief Used to parse a true or false value (usually from an argument)
	 * \param str The string to examine
	 * \returns true for strings like "true", "True", "TRUE", "t", "T", "y", "Y", "yes", "Yes", "1", or
	 *          false for strings like "false", "False", "FALSE", "f", "F", "n", "N", "no", "No", "0"
	 * \throws InvalidInputException if the string matches neither of these groups
	 */
	bool getStringTruthValue(const std::string& str);

	//! The loop that pareses through an entire character sequence specified by the provided Parser
	void parseLoop(bool createReplacements);

	//! Reads tabs and spaces until a non-whitespace character or a newline is hit
	inline void eatWhitespace()
	{
		while (curr < end && std::isblank(*curr))
			++curr;
	}

	//! Tries to read a newline at the current location
	//! \returns true if a newline was read
	bool readNewline();

	//! Called when we hit a \\include or \\input
	//! When the function returns, p.curr is moved past the \\include statement
	void processInclude();

	/*!
	 * \brief Parses SemTeX macro options (e.g. \\macro[these]{not, these}).
	 * \returns A heap-allocated MacroArgs struct containing the arguments
	 * \todo Would stack allocation be better?
	 *
	 * When the function returns, curr is moved past the arguments
	 */
	std::unique_ptr<MacroOptions> parseMacroOptions();

	std::unique_ptr<std::vector<std::string>> parseBracketArgs();

	/*!
	 * \brief A method for throwing standardized exceptions for input errors
	 * \param msg The error-specific message to attach to the exception
	 */
	void errorOnLine(const std::string& msg);

	// No copy or assignment
	Parser(const Parser&) = delete;
	Parser& operator=(const Parser&) = delete;

private:
	const std::string filename;
	int currLine;
	int unixNewlines;
	int windowsNewlines;
	int macNewlines;
	Context& ctxt;
};

/*!
 * \brief Processes a SemTeX file, generating a corresponding LaTeX file and adding included SemTeX files
 *        to the queue
 * \param filename The path of the SemTeX file to process
 * \param ctxt The global context (verbosity level, queues, etc.)
 */
void processFile(const std::string& filename, Context& ctxt);
#endif
