#load "..\testFramework.csx"

Test("Dialogs", () =>
{
    ExecuteCommand("test_dialogs_client");
});

Test("Dialogs", () =>
{
    ExecuteCommand("test_dialogs_client_gump_generation");
    UO.LastGumpInfo();
});

Test("Dialogs - trigger specific button", () =>
{
    ExecuteCommand("test_dialogs_specific_button_client");
    UO.GumpResponse()
        .SelectCheckBox((GumpControlId)1)
        .Trigger((GumpControlId)1);
});

Test("Dialogs - trigger any button", () =>
{
    ExecuteCommand("test_dialogs_any_button_client");
    UO.TriggerGump(3);
});