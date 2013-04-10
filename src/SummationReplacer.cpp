#include "precomp.hpp"

#include "SummationReplacer.hpp"

#include "Exceptions.hpp"
#include "FileParser.hpp"

static std::unordered_set<std::string> acceptedFlags({"inf", "lim"});

SummationReplacer::SummationReplacer()
	: Replacer({"\\summ"})
{ }

void SummationReplacer::replace(const std::string& matchedKey, ParseInfo& pi)
{
	const char* start = pi.curr;
	pi.curr += matchedKey.length();

	std::unique_ptr<MacroOptions> options;
	decltype(pi.parseBracketArgs()) argList;
	try {
		options = pi.parseMacroOptions();
		argList = pi.parseBracketArgs();
	}
	catch (const Exceptions::InvalidInputException& ex) {
		throw Exceptions::InvalidInputException(ex.message + " in \\summation", __FUNCTION__);
	}

	if (options->opts.size() != 0)
		pi.errorOnLine("\\summation does not take options");

	for (const auto& flag : options->flags) {
		if (acceptedFlags.find(flag) == acceptedFlags.end())
			pi.errorOnLine("Unknown argument \"" + flag + "\" for \\summation");
	}

	const size_t numArgs = argList->size();

	if (numArgs > 3)
		pi.errorOnLine("Too many arguments for \\summation");

	bool inf = options->flags.find("inf") != options->flags.end();
	bool lim = options->flags.find("lim") != options->flags.end();

	// Arg 0 is the counting variable
	// Arg 1 is the lower bound
	// Arg 2 is the upper bound
	const std::string* wrt = numArgs >= 1 ? &argList->at(0) : nullptr;
	const std::string* lower = numArgs >= 2 && !argList->at(1).empty() ? &argList->at(1) : nullptr;
	const std::string* upper = numArgs >= 3 && !argList->at(2).empty() ? &argList->at(2) : nullptr;

	std::string replacement = "\\sum";
	if ((upper != nullptr || lower != nullptr || inf) && lim)
		replacement += "\\limits";

	if (lower != nullptr)
		replacement += "_{" + (wrt != nullptr ? *wrt + "=" : "") + *lower + "}";
	else if (inf)
		replacement += "_{"+ (wrt != nullptr ? *wrt + "=" : "") + "-\\infty}";
	else if (wrt != nullptr)
		replacement += "_{" + *wrt + "}";

	if (upper != nullptr)
		replacement += "^{" + *upper + "}";
	else if (inf)
		replacement += "^{\\infty}";

	pi.replacements.emplace_back(start, pi.curr, std::move(replacement));
}
