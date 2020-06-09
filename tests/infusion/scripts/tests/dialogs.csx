#load "..\testFramework.csx"

// Test("Dialogs", () =>
// {
//     ExecuteCommand("test_dialogs_client")
// });


Test("Dialogs", () =>
{
    ExecuteCommand("test_dialogs_client_gump_generation");
    UO.LastGumpInfo();
});