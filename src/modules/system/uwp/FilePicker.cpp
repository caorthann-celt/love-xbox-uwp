#include "FilePicker.h"

#include <mutex>

#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.UI.Core.h>

namespace love
{
namespace system
{
namespace uwp
{

namespace
{

std::mutex pickerMutex;
bool pickerPending = false;
std::string pickedFile;
std::string pickerError;

void finishPicker(const std::string &path, const std::string &error = std::string())
{
	std::lock_guard<std::mutex> lock(pickerMutex);
	pickerPending = false;
	pickedFile = path;
	pickerError = error;
}

void finishPicker(winrt::hresult error)
{
	finishPicker(std::string(), winrt::to_string(winrt::hresult_error(error).message()));
}

void openPicker(const std::string &kind)
{
	namespace Foundation = winrt::Windows::Foundation;
	namespace Storage = winrt::Windows::Storage;
	namespace Pickers = winrt::Windows::Storage::Pickers;

	try
	{
		Pickers::FileOpenPicker picker;
		picker.SuggestedStartLocation(Pickers::PickerLocationId::ComputerFolder);

		auto filters = picker.FileTypeFilter();
		std::string destination;
		if (kind == "mod")
		{
			filters.Append(L".zip");
			destination = "picked_mod.zip";
		}
		else if (kind == "sav")
		{
			filters.Append(L".sav");
			destination = "picked_save.sav";
		}
		else
		{
			filters.Append(L".gb");
			filters.Append(L".gbc");
			destination = "picked_rom.gb";
		}

		auto operation = picker.PickSingleFileAsync();
		operation.Completed([destination](auto const &operation, Foundation::AsyncStatus status)
		{
			try
			{
				if (status == Foundation::AsyncStatus::Canceled)
				{
					finishPicker(std::string());
					return;
				}
				if (status != Foundation::AsyncStatus::Completed)
				{
					finishPicker(operation.ErrorCode());
					return;
				}

				Storage::StorageFile selected = operation.GetResults();
				if (!selected)
				{
					finishPicker(std::string());
					return;
				}

				// Keep brokered files in app-owned storage after the picker closes.
				auto localFolder = Storage::ApplicationData::Current().LocalFolder();
				auto copy = selected.CopyAsync(localFolder, winrt::to_hstring(destination),
					Storage::NameCollisionOption::ReplaceExisting);
				copy.Completed([](auto const &copy, Foundation::AsyncStatus copyStatus)
				{
					try
					{
						if (copyStatus == Foundation::AsyncStatus::Completed)
						{
							Storage::StorageFile localFile = copy.GetResults();
							finishPicker(winrt::to_string(localFile.Path()));
						}
						else if (copyStatus == Foundation::AsyncStatus::Canceled)
							finishPicker(std::string());
						else
							finishPicker(copy.ErrorCode());
					}
					catch (const winrt::hresult_error &e)
					{
						finishPicker(std::string(), winrt::to_string(e.message()));
					}
				});
			}
			catch (const winrt::hresult_error &e)
			{
				finishPicker(std::string(), winrt::to_string(e.message()));
			}
		});
	}
	catch (const winrt::hresult_error &e)
	{
		finishPicker(std::string(), winrt::to_string(e.message()));
	}
}

} // namespace

bool pickFile(const char *kind)
{
	std::string requestedKind = kind ? kind : "rom";
	if (requestedKind != "rom" && requestedKind != "mod" && requestedKind != "sav")
		return false;

	{
		std::lock_guard<std::mutex> lock(pickerMutex);
		if (pickerPending)
			return true;
		pickerPending = true;
		pickedFile.clear();
		pickerError.clear();
	}

	try
	{
		auto dispatcher = winrt::Windows::ApplicationModel::Core::CoreApplication::MainView()
			.CoreWindow().Dispatcher();
		dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
			[requestedKind]() { openPicker(requestedKind); });
	}
	catch (const winrt::hresult_error &e)
	{
		finishPicker(std::string(), winrt::to_string(e.message()));
		return false;
	}

	return true;
}

bool takePickedFile(std::string &path)
{
	std::lock_guard<std::mutex> lock(pickerMutex);
	if (pickedFile.empty())
		return false;
	path.swap(pickedFile);
	return true;
}

bool takePickError(std::string &error)
{
	std::lock_guard<std::mutex> lock(pickerMutex);
	if (pickerError.empty())
		return false;
	error.swap(pickerError);
	return true;
}

} // uwp
} // system
} // love
