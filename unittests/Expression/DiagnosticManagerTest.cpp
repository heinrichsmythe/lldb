//===-- DiagnosticManagerTest.cpp --------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Expression/DiagnosticManager.h"
#include "gtest/gtest.h"

using namespace lldb_private;

static const uint32_t custom_diag_id = 42;

namespace {
class FixItDiag : public Diagnostic {
  bool m_has_fixits;

public:
  FixItDiag(llvm::StringRef msg, bool has_fixits)
      : Diagnostic(msg, DiagnosticSeverity::eDiagnosticSeverityError,
                   DiagnosticOrigin::eDiagnosticOriginLLDB, custom_diag_id),
        m_has_fixits(has_fixits) {}
  bool HasFixIts() const override { return m_has_fixits; }
};
} // namespace

namespace {
class TextDiag : public Diagnostic {
public:
  TextDiag(llvm::StringRef msg, DiagnosticSeverity severity)
      : Diagnostic(msg, severity, DiagnosticOrigin::eDiagnosticOriginLLDB,
                   custom_diag_id) {}
};
} // namespace

TEST(DiagnosticManagerTest, AddDiagnostic) {
  DiagnosticManager mgr;
  EXPECT_EQ(0U, mgr.Diagnostics().size());

  Diagnostic *diag = new Diagnostic(
      "foo bar has happened", DiagnosticSeverity::eDiagnosticSeverityError,
      DiagnosticOrigin::eDiagnosticOriginLLDB, custom_diag_id);
  mgr.AddDiagnostic(diag);
  EXPECT_EQ(1U, mgr.Diagnostics().size());
  Diagnostic *got = mgr.Diagnostics().front();
  EXPECT_EQ(diag->getKind(), got->getKind());
  EXPECT_EQ(diag->GetMessage(), got->GetMessage());
  EXPECT_EQ(diag->GetSeverity(), got->GetSeverity());
  EXPECT_EQ(diag->GetCompilerID(), got->GetCompilerID());
  EXPECT_EQ(diag->HasFixIts(), got->HasFixIts());
}

TEST(DiagnosticManagerTest, HasFixits) {
  DiagnosticManager mgr;
  // By default we shouldn't have any fixits.
  EXPECT_FALSE(mgr.HasFixIts());
  // Adding a diag without fixits shouldn't make HasFixIts return true.
  mgr.AddDiagnostic(new FixItDiag("no fixit", false));
  EXPECT_FALSE(mgr.HasFixIts());
  // Adding a diag with fixits will mark the manager as containing fixits.
  mgr.AddDiagnostic(new FixItDiag("fixit", true));
  EXPECT_TRUE(mgr.HasFixIts());
  // Adding another diag without fixit shouldn't make it return false.
  mgr.AddDiagnostic(new FixItDiag("no fixit", false));
  EXPECT_TRUE(mgr.HasFixIts());
  // Adding a diag with fixits. The manager should still return true.
  mgr.AddDiagnostic(new FixItDiag("fixit", true));
  EXPECT_TRUE(mgr.HasFixIts());
}

TEST(DiagnosticManagerTest, GetStringNoDiags) {
  DiagnosticManager mgr;
  EXPECT_EQ("", mgr.GetString());
}

TEST(DiagnosticManagerTest, GetStringBasic) {
  DiagnosticManager mgr;
  mgr.AddDiagnostic(new TextDiag("abc", eDiagnosticSeverityError));
  EXPECT_EQ("error: abc\n", mgr.GetString());
}

TEST(DiagnosticManagerTest, GetStringMultiline) {
  DiagnosticManager mgr;

  // Multiline diagnostics should only get one severity label.
  mgr.AddDiagnostic(new TextDiag("b\nc", eDiagnosticSeverityError));
  EXPECT_EQ("error: b\nc\n", mgr.GetString());
}

TEST(DiagnosticManagerTest, GetStringMultipleDiags) {
  DiagnosticManager mgr;
  mgr.AddDiagnostic(new TextDiag("abc", eDiagnosticSeverityError));
  EXPECT_EQ("error: abc\n", mgr.GetString());
  mgr.AddDiagnostic(new TextDiag("def", eDiagnosticSeverityError));
  EXPECT_EQ("error: abc\nerror: def\n", mgr.GetString());
}

TEST(DiagnosticManagerTest, GetStringSeverityLabels) {
  DiagnosticManager mgr;

  // Different severities should cause different labels.
  mgr.AddDiagnostic(new TextDiag("foo", eDiagnosticSeverityError));
  mgr.AddDiagnostic(new TextDiag("bar", eDiagnosticSeverityWarning));
  // Remarks have no labels.
  mgr.AddDiagnostic(new TextDiag("baz", eDiagnosticSeverityRemark));
  EXPECT_EQ("error: foo\nwarning: bar\nbaz\n", mgr.GetString());
}

TEST(DiagnosticManagerTest, GetStringPreserveOrder) {
  DiagnosticManager mgr;

  // Make sure we preserve the diagnostic order and do not sort them in any way.
  mgr.AddDiagnostic(new TextDiag("baz", eDiagnosticSeverityRemark));
  mgr.AddDiagnostic(new TextDiag("bar", eDiagnosticSeverityWarning));
  mgr.AddDiagnostic(new TextDiag("foo", eDiagnosticSeverityError));
  EXPECT_EQ("baz\nwarning: bar\nerror: foo\n", mgr.GetString());
}

TEST(DiagnosticManagerTest, AppendMessageNoDiag) {
  DiagnosticManager mgr;

  // FIXME: This *really* should not just fail silently.
  mgr.AppendMessageToDiagnostic("no diag has been pushed yet");
  EXPECT_EQ(0U, mgr.Diagnostics().size());
}

TEST(DiagnosticManagerTest, AppendMessageAttachToLastDiag) {
  DiagnosticManager mgr;

  mgr.AddDiagnostic(new TextDiag("foo", eDiagnosticSeverityError));
  mgr.AddDiagnostic(new TextDiag("bar", eDiagnosticSeverityError));
  // This should append to 'bar' and not to 'foo'.
  mgr.AppendMessageToDiagnostic("message text");

  EXPECT_EQ("error: foo\nerror: bar\nmessage text\n", mgr.GetString());
}

TEST(DiagnosticManagerTest, AppendMessageSubsequentDiags) {
  DiagnosticManager mgr;

  mgr.AddDiagnostic(new TextDiag("bar", eDiagnosticSeverityError));
  mgr.AppendMessageToDiagnostic("message text");
  // Pushing another diag after the message should work fine.
  mgr.AddDiagnostic(new TextDiag("foo", eDiagnosticSeverityError));

  EXPECT_EQ("error: bar\nmessage text\nerror: foo\n", mgr.GetString());
}

TEST(DiagnosticManagerTest, PutString) {
  DiagnosticManager mgr;

  mgr.PutString(eDiagnosticSeverityError, "foo");
  EXPECT_EQ(1U, mgr.Diagnostics().size());
  EXPECT_EQ(eDiagnosticOriginLLDB, mgr.Diagnostics().front()->getKind());
  EXPECT_EQ("error: foo\n", mgr.GetString());
}

TEST(DiagnosticManagerTest, PutStringMultiple) {
  DiagnosticManager mgr;

  // Multiple PutString should behave like multiple diagnostics.
  mgr.PutString(eDiagnosticSeverityError, "foo");
  mgr.PutString(eDiagnosticSeverityError, "bar");
  EXPECT_EQ(2U, mgr.Diagnostics().size());
  EXPECT_EQ("error: foo\nerror: bar\n", mgr.GetString());
}

TEST(DiagnosticManagerTest, PutStringSeverities) {
  DiagnosticManager mgr;

  // Multiple PutString with different severities should behave like we
  // created multiple diagnostics.
  mgr.PutString(eDiagnosticSeverityError, "foo");
  mgr.PutString(eDiagnosticSeverityWarning, "bar");
  EXPECT_EQ(2U, mgr.Diagnostics().size());
  EXPECT_EQ("error: foo\nwarning: bar\n", mgr.GetString());
}

TEST(DiagnosticManagerTest, FixedExpression) {
  DiagnosticManager mgr;

  // By default there should be no fixed expression.
  EXPECT_EQ("", mgr.GetFixedExpression());

  // Setting the fixed expression should change it.
  mgr.SetFixedExpression("foo");
  EXPECT_EQ("foo", mgr.GetFixedExpression());

  // Setting the fixed expression again should also change it.
  mgr.SetFixedExpression("bar");
  EXPECT_EQ("bar", mgr.GetFixedExpression());
}
