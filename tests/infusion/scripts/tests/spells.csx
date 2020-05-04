#load "..\testFramework.csx"

Test("Spell effects", () =>
{
    ExecuteCommand("test_spelleffect_invisibility");
    ShouldBeTrue("s_invisibility spelleffect hides player", UO.Me.IsHidden);
});