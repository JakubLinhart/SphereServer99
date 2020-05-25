#load "..\testFramework.csx"

Test("Dialogs", () =>
{
    ExecuteCommand("test_dialog");
    UO.LastGumpInfo();
});