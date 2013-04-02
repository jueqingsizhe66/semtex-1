#include "precomp.hpp"

#include "Context.hpp"
#include "Exceptions.hpp"
#include "FileParser.hpp"
#include "FileQueue.hpp"
#include "ProcessorThread.hpp"

// Prototype for the function below so we can declare ctxt with the other static variables.
// Slightly kludgy, I know.
void queueCallback(const FileQueue& sfq);

namespace { // Ensure these variables are accessible only within this file.
	bool threadsStarted = false;
	std::vector<std::unique_ptr<ProcessorThread>> auxThreads;
	Context ctxt(&queueCallback);
}

void queueCallback(const FileQueue& sfq)
{
	if (threadsStarted)
		return;

	unsigned int numThreads = std::max(2u, std::thread::hardware_concurrency());

	if (ctxt.verbose)
		printf("Processing multiple files. Starting up %u additional threads.\n", numThreads);

	for (unsigned int n = 0; n < numThreads; ++n)
		auxThreads.emplace_back(new ProcessorThread(ctxt));

	threadsStarted = true;
}

int main(int argc, char** argv) {
	TCLAP::SwitchArg verbFlag("v", "verbose", "Print additional output");
	TCLAP::SwitchArg keepFlag("k", "keep-tex",
	                          "Don't delete the LaTeX files generated by SemTeX after pdflatex has run");
	// -E matches gcc
	TCLAP::SwitchArg preOnlyFlag("E", "preprocess-only",
	                             "Just process files and output LaTeX ones instead of running pdflatex. Implies -k");
	TCLAP::UnlabeledValueArg<std::string> fileArg("file", "Base SemTeX file", true, "",  "file");

	TCLAP::CmdLine cmd("SemTeX - Streamlined LaTeX", ' ', "alpha");
	cmd.add(verbFlag);
	cmd.add(keepFlag);
	cmd.add(preOnlyFlag);
	cmd.add(fileArg);

	cmd.parse(argc, argv);

	ctxt.verbose = verbFlag.getValue();

	if (ctxt.verbose)
		printf("Running SemTex - Streamlined LaTeX\n");

	try {
		processFile(fileArg.getValue(), ctxt);
	}
	catch (const Exceptions::Exception& ex) {
		ctxt.error = true;
		fprintf(stderr, "%s\n", ex.message.c_str());
	}
	catch (const std::exception& ex) {
		ctxt.error = true;
		fprintf(stderr, "Unexpected fatal error: %s\n", ex.what());
	}
	catch (...) {
		ctxt.error = true;
		fprintf(stderr, "Unexpected fatal error");
	}

	if (threadsStarted) {
		// Wait for the threads to finish doing their thing
		// See FileQueue::setDequeueEnabled for an explanation on why we are disabling dequeuing here
		while (true) {
			ctxt.queue.setDequeueEnabled(false);
			bool done = std::none_of(auxThreads.begin(), auxThreads.end(),
									 [](const std::unique_ptr<ProcessorThread>& pt) { return pt->isBusy(); } );
			done = done && (ctxt.queue.empty() || ctxt.error);
			ctxt.queue.setDequeueEnabled(true);
			if (done)
				break;
			std::this_thread::sleep_for(ProcessorThread::dequeueTimeout / 2);
		}

		for (auto& thread : auxThreads)
			thread->beginExit();
	}

	if (!ctxt.error) {
		if (!preOnlyFlag.getValue()) {
			if (ctxt.verbose)
				printf("Running pdflatex...\n");

			//! \todo Move this into a function? This is the second place we use it
			boost::regex fext(R"regex((stex|sex)$)regex", boost::regex::optimize);
			const std::string texname = boost::regex_replace(fileArg.getValue(), fext, "tex");

			fflush(stdout); // Make sure everything prints before LaTeX does

			// TODO: handle pdflatex I/O instead of just calling it
			system(("pdflatex " + texname).c_str());

			if (ctxt.verbose)
				printf("pdflatex exited.\n");
		}
	}
	else
	{
		if (ctxt.verbose)
			printf("Skipping pdflatex due to errors\n");
	}

	if (!keepFlag.getValue() && !preOnlyFlag.getValue()) {
		std::lock_guard<std::mutex> genLock(ctxt.generatedFilesMutex);
		for (const std::string& f : ctxt.generatedFiles) {
			if (ctxt.verbose)
				printf("Removing intermediate LaTeX file %s\n", f.c_str());

			boost::filesystem::remove(f);
		}
	}

	if (threadsStarted) {
		for (auto& thread : auxThreads)
			thread->join();
	}
	return 0;
}