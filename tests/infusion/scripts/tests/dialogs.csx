#load "..\testFramework.csx"

Test("Dialogs", () =>
{
    ExecuteCommand("test_dialogs_client");
});

Test("Dialogs - control generation", () =>
{
    ExecuteCommand("test_dialogs_client_gump_generation");
    LastGumpInfo();
});

Test("Dialogs - text generation", () =>
{
    ExecuteCommand("test_dialogs_client_gump_settexts");
    LastGumpInfo();
});

Test("Dialogs - trigger specific button", () =>
{
    ExecuteCommand("test_dialogs_specific_button_client");

    UO.GumpResponse()
        .SetTextEntry((GumpControlId)10,"this comes from client")
        .SelectCheckBox((GumpControlId)1)
        .Trigger((GumpControlId)1);
});

Test("Dialogs - trigger any button", () =>
{
    ExecuteCommand("test_dialogs_any_button_client");
    UO.TriggerGump(3);
});