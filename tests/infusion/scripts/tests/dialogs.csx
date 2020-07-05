#load "..\testFramework.csx"
using System.Linq;

Test("Dialogs", () =>
{
    ExecuteCommand("test_dialogs_client");
});

Test("Dialogs - trigger specific button", () =>
{
    ExecuteCommand("test_dialogs_specific_button_client");

    UO.GumpResponse()
        .SetTextEntry((GumpControlId)10,"this comes from client")
        .SelectCheckBox((GumpControlId)1)
        .Trigger((GumpControlId)1);
});

Test("Dialogs - trigger specific button, single textentry", () =>
{
    ExecuteCommand("test_dialogs_specific_single_textentry_client");
    UO.GumpResponse()
        .SetTextEntry((GumpControlId)0,"this comes from client - single textentry")
        .Trigger((GumpControlId)1);
});

Test("Dialogs - trigger any button", () =>
{
    ExecuteCommand("test_dialogs_any_button_client");
    UO.TriggerGump(3);
});

Test("Dialogs - control generation", () =>
{
    ExecuteCommand("test_dialogs_client_gump_generation");
    LastGumpInfo();

    ShouldBeEqual("static text from text section", "this is static text", UO.CurrentGump.TextLines[0]);
    ShouldBeEqual("text with macro from text section", "Variable value: this comes from setup variable", UO.CurrentGump.TextLines[1]);

    var gumpIdGeneratedFromDefName = uint.Parse(UO.CurrentGump.TextLines[2], System.Globalization.NumberStyles.HexNumber);
    var gumpIdSentToClient = UO.CurrentGump.GumpTypeId.Value;
    ShouldBeEqual("defname value should be sent to client", gumpIdGeneratedFromDefName, gumpIdSentToClient);
});

Test("Dialogs - text generation", () =>
{
    ExecuteCommand("test_dialogs_client_gump_settexts");

    ShouldBeEqual("settext for index 0", "this is first text at index 0", UO.CurrentGump.TextLines[0]);
    ShouldBeEqual("settext for index 10", "this is second text at index 10", UO.CurrentGump.TextLines[10]);
    ShouldBeTrue("unset texts are empty", UO.CurrentGump.TextLines.Skip(1).Take(9).All(string.IsNullOrEmpty));
    ShouldBeEqual("total text lines count", 11, UO.CurrentGump.TextLines.Length);
});
