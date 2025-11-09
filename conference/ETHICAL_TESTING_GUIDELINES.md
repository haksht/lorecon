# Ethical Testing Guidelines for Offensive Security Research

**Version:** 1.0  
**Last Updated:** November 9, 2025  
**Scope:** LoRa/Meshtastic device security assessment

---

## 🎯 Purpose

This document establishes ethical boundaries for using offensive security testing capabilities in the ESP32 LoRa Sniffer. These guidelines ensure research is conducted responsibly, legally, and for defensive purposes.

---

## ⚖️ Core Principles

### 1. **Authorization First**

✅ **ALWAYS REQUIRED:**
- Written permission from device owner
- Authorization from network operator
- Clear scope of testing defined
- Documented testing agreement

❌ **NEVER ACCEPTABLE:**
- Testing devices you don't own
- Testing without explicit permission
- "Assumed" authorization
- Testing in public spaces without consent

### 2. **Minimize Harm**

✅ **DO:**
- Use minimal power necessary
- Limit test duration
- Test during agreed timeframes
- Have rollback procedures ready

❌ **DON'T:**
- Intentionally cause permanent damage
- Disrupt emergency communications
- Interfere with critical infrastructure
- Continue testing after issues detected

### 3. **Responsible Disclosure**

✅ **PROPER DISCLOSURE:**
- 90-day vendor notification period
- Provide detailed technical reports
- Offer to help develop patches
- Coordinate public disclosure timing

❌ **IMPROPER DISCLOSURE:**
- Immediate public exploit release
- Weaponizing vulnerabilities
- Selling exploits to bad actors
- Ignoring vendor communication

---

## 📋 Pre-Testing Checklist

Before conducting any offensive security tests:

- [ ] **Authorization documented** (written agreement on file)
- [ ] **Scope clearly defined** (what systems, what tests, what timeframe)
- [ ] **Emergency contact established** (who to call if something goes wrong)
- [ ] **Backup plan ready** (how to restore service if disrupted)
- [ ] **Logging enabled** (audit trail of all actions)
- [ ] **Legal counsel consulted** (if testing crosses jurisdictions)
- [ ] **Insurance verified** (liability coverage adequate)
- [ ] **Notification sent** (stakeholders aware testing is happening)

---

## 🚦 Testing Approval Levels

### **Level 1: Non-Destructive (Green Light)**
Tests that cannot harm target or network:
- Passive monitoring
- Traffic analysis
- Signal strength measurements
- Protocol observation

**Authorization Required:** Your own devices only

### **Level 2: Active Probing (Yellow Light)**
Tests that send packets but don't disrupt:
- Timing analysis
- Single packet tests
- Response time measurements
- Protocol conformance checks

**Authorization Required:** Device owner permission

### **Level 3: Stress Testing (Orange Light)**
Tests that may cause temporary degradation:
- Packet flooding (controlled)
- Malformed packet fuzzing
- Resource exhaustion tests
- Protocol violation tests

**Authorization Required:** Network operator + device owner

### **Level 4: Destructive Testing (Red Light)**
Tests that may cause crashes or outages:
- Sustained flooding attacks
- Frequency jamming
- Routing disruption
- Identity spoofing

**Authorization Required:** Full written agreement + legal review

---

## 🛡️ Safety Protocols

### **Before Testing**

1. **Verify authorization documents**
2. **Check emergency contact availability**
3. **Ensure monitoring is in place**
4. **Set time limits on tests**
5. **Prepare recovery procedures**

### **During Testing**

1. **Monitor target continuously**
2. **Stop immediately if unintended effects**
3. **Feed watchdog timer regularly**
4. **Log all actions taken**
5. **Stay within approved scope**

### **After Testing**

1. **Verify target recovery**
2. **Document all findings**
3. **Secure test data**
4. **Notify stakeholders of results**
5. **Initiate disclosure process**

---

## 📝 Documentation Requirements

### **Testing Plan (Before)**

```markdown
# Security Testing Plan

**Target:** [Device model, firmware version]
**Authorization:** [Reference to written agreement]
**Scope:** [Specific tests to be conducted]
**Duration:** [Start/end dates and times]
**Contacts:** 
  - Tester: [Name, email, phone]
  - Device Owner: [Name, email, phone]
  - Emergency: [24/7 contact]

**Tests Approved:**
- [ ] Test 1: [Description]
- [ ] Test 2: [Description]

**Limitations:**
- Maximum packet rate: [X packets/sec]
- Maximum test duration: [X minutes]
- Prohibited actions: [List]

**Rollback Plan:**
[How to restore service if disrupted]
```

### **Testing Log (During)**

```markdown
# Security Testing Log

**Date/Time:** [Timestamp]
**Tester:** [Name]
**Action:** [What was done]
**Result:** [Outcome observed]
**Issues:** [Any problems encountered]
```

### **Final Report (After)**

```markdown
# Security Assessment Report

**Executive Summary:**
[High-level findings]

**Vulnerabilities Discovered:**
1. [Vulnerability name]
   - Severity: [CRITICAL/HIGH/MEDIUM/LOW]
   - Description: [Technical details]
   - Impact: [What attacker could achieve]
   - Recommendation: [How to fix]

**Test Methodology:**
[What tests were conducted]

**Timeline:**
- Testing: [Dates]
- Report Delivered: [Date]
- Disclosure Date: [90 days from delivery]
```

---

## 🚨 Incident Response

### **If Something Goes Wrong**

1. **STOP ALL TESTING IMMEDIATELY**
2. Contact emergency contact
3. Document what happened
4. Assist with recovery efforts
5. File incident report
6. Review authorization and procedures

### **Reportable Incidents**

Must be reported immediately:
- Target device crashed or bricked
- Network outage caused
- Unintended third parties affected
- Data loss or corruption
- Physical damage to equipment
- Legal concerns raised

---

## ⚠️ Legal Considerations

### **Know Your Jurisdiction**

Different regions have different laws regarding:
- Computer Fraud and Abuse
- Telecommunications regulations
- Privacy laws
- Wireless interference

**Consult Legal Counsel** if:
- Testing crosses state/country borders
- Critical infrastructure involved
- Commercial products being tested
- Results will be published
- Any uncertainty about legality

### **Criminal vs. Authorized Testing**

**Authorized Security Research:**
- Written permission obtained
- Scope clearly defined
- Good-faith effort to improve security
- Responsible disclosure followed

**Unauthorized Hacking:**
- No permission
- Intent to harm
- Extortion or blackmail
- Public exploit release

---

## 🎓 Researcher Code of Conduct

### **Professional Standards**

As a security researcher, I commit to:

1. **Obtain authorization** before testing any system
2. **Minimize harm** in all security assessments
3. **Disclose responsibly** to affected parties first
4. **Respect privacy** of users and communications
5. **Maintain confidentiality** of sensitive findings
6. **Educate and improve** the security community
7. **Follow the law** in all jurisdictions involved
8. **Act in good faith** to improve security, not exploit it

### **Prohibited Actions**

I will never:
- Test systems without authorization
- Sell vulnerabilities to criminal organizations
- Use findings for personal gain through extortion
- Disclose vulnerabilities before vendor has time to patch
- Retain or distribute sensitive data obtained during testing
- Continue testing after being asked to stop

---

## 📊 Disclosure Process

### **Standard Timeline**

```
Day 0:   Vulnerability discovered
Day 1:   Initial vendor contact (encrypted email)
Day 7:   Detailed technical report submitted
Day 14:  Vendor acknowledges receipt
Day 30:  Status update requested
Day 60:  Patch development progress check
Day 90:  Public disclosure (if vendor has not patched)
```

### **Vendor Communication Template**

```
Subject: [SECURITY] Vulnerability Report - [Product Name]

Dear Security Team,

I am a security researcher and have discovered a vulnerability 
in [product name] version [X.Y.Z].

Severity: [CRITICAL/HIGH/MEDIUM/LOW]
Impact: [Brief description]

I would like to follow responsible disclosure practices and 
give you 90 days to develop a patch before public disclosure.

Detailed technical report is attached (encrypted).

Please confirm receipt and let me know the best way to coordinate.

Thank you,
[Your Name]
[Contact Information]
```

---

## 🔒 Data Handling

### **Sensitive Data**

If testing reveals sensitive information:

✅ **DO:**
- Encrypt all findings immediately
- Limit access to need-to-know basis
- Delete after disclosure complete
- Use secure communication channels

❌ **DON'T:**
- Store on unsecured devices
- Share via unencrypted email
- Post publicly before disclosure
- Retain indefinitely

---

## 🏆 Recognition and Attribution

### **Responsible Recognition**

It's acceptable to:
- Request CVE assignment
- Accept bug bounty rewards
- Present at conferences (post-disclosure)
- Publish technical write-ups (post-patch)
- Include in resume/portfolio

It's not acceptable to:
- Demand payment for disclosure
- Threaten public release for recognition
- Compete for "first to exploit"
- Publicize for ego before patch

---

## 📞 Emergency Contacts

### **If You Witness Abuse**

Report unethical security research to:
- CERT Coordination Center (cert.org)
- Local law enforcement (for crimes)
- Professional organizations (IEEE, ACM)

### **Get Help**

If you're unsure about ethics of a test:
- Consult legal counsel
- Contact CERT/CC for guidance
- Reach out to HackerOne or similar platforms
- Ask experienced researchers

---

## ✅ Testing Readiness Self-Assessment

Before conducting offensive security tests, honestly answer:

1. **Do I have written authorization?** YES / NO
2. **Have I defined the scope clearly?** YES / NO
3. **Do I know the emergency contact?** YES / NO
4. **Have I prepared rollback procedures?** YES / NO
5. **Will I log all my actions?** YES / NO
6. **Am I prepared to disclose responsibly?** YES / NO
7. **Will this harm anyone?** YES / NO
8. **Is this legal in my jurisdiction?** YES / NO

**If ANY "NO" answers (or "YES" to #7), DO NOT PROCEED.**

---

## 📚 Additional Resources

### **Responsible Disclosure**
- CERT Guide to Coordinated Vulnerability Disclosure: https://vuls.cert.org/confluence/
- HackerOne Disclosure Guidelines: https://www.hackerone.com/disclosure-guidelines
- ISO 29147: Vulnerability disclosure

### **Legal Guidance**
- EFF Coders' Rights Project: https://www.eff.org/issues/coders
- CFAA Reform: https://www.eff.org/issues/cfaa
- Your local laws regarding computer security research

### **Community Standards**
- OWASP Testing Guide: https://owasp.org/www-project-web-security-testing-guide/
- Bugcrowd Vulnerability Rating Taxonomy: https://bugcrowd.com/vulnerability-rating-taxonomy

---

## 🎯 Summary

**Golden Rules:**

1. **Authorization is mandatory**
2. **Minimize harm always**
3. **Disclose responsibly**
4. **Document everything**
5. **Follow the law**
6. **Act in good faith**

**Remember:** The goal is to make systems more secure, not to cause harm or gain unauthorized access. When in doubt, don't test - seek guidance first.

---

**Acknowledgment:**

By using the offensive security testing features in this tool, I acknowledge that I have read, understood, and agree to follow these ethical guidelines. I understand that misuse of these capabilities may result in criminal prosecution and civil liability.

**I commit to using these tools only for authorized security research and defensive purposes.**

---

**For questions about these guidelines, contact:**
- Project maintainer: [GitHub issues]
- Legal questions: [Consult your attorney]
- Ethics questions: [CERT/CC, EFF, or similar organizations]

**Version History:**
- v1.0 (Nov 9, 2025): Initial release

