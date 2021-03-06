#load "startup.csx"

using System;

public void Test(string name, Action testAction)
{
    UO.Console.Info("");
    UO.Console.Important($"Starting {name}...");

    Startup.RunInGame(testAction);
}

public void ExecuteCommand(string cmd)
{
    UO.Say("." + cmd);
    UO.Journal.WaitAny(TimeSpan.FromSeconds(1), cmd + " done");
}

public void Success(string text)
{
    UO.Console.Info("        OK - " + text);
}

public void Fail(string text)
{
    UO.Console.Error("    FAILED - " + text);
}

public void ShouldBeTrue(string comment, bool result)
{
    if (result)
        Success(comment);
    else
        Fail(comment);
}

public void ShouldBeEqual(string comment, object expected, object actual)
{
    if (expected.Equals(actual))
        Success(comment);
    else
        Fail($"{comment}: {actual} is expected to be {expected}");
}

public void LastGumpInfo()
{
    UO.LastGumpInfo();

    UO.Log("Texts:");
    var i = 0;
    foreach (var text in UO.CurrentGump.TextLines)
    {
        if (!string.IsNullOrEmpty(text))
            UO.Log($"{i}: {text}");
        i++;
    }
}